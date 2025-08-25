#include "sim.h"
#include <math.h>
#include <stdlib.h>

#ifdef _OPENMP
  #include <omp.h>
#endif

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline void hsv_to_rgb(float h, float s, float v, uint8_t* R, uint8_t* G, uint8_t* B) {
    h = h - floorf(h);
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float r,g,b;
    switch (((int)i) % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        default: r=v; g=p; b=q; break;
    }
    *R = (uint8_t)(clampf(r,0,1)*255.0f);
    *G = (uint8_t)(clampf(g,0,1)*255.0f);
    *B = (uint8_t)(clampf(b,0,1)*255.0f);
}

void init_particles(Particle* p, int N, unsigned int seed) {
    srand(seed);
    for (int i=0; i<N; ++i) {
        float fr  = (float)rand()/(float)RAND_MAX;
        float fr2 = (float)rand()/(float)RAND_MAX;
        float fr3 = (float)rand()/(float)RAND_MAX;
        float fr4 = (float)rand()/(float)RAND_MAX;
        float fv1 = (float)rand()/(float)RAND_MAX;
        float fv2 = (float)rand()/(float)RAND_MAX;
        float fv3 = (float)rand()/(float)RAND_MAX;
        float fv4 = (float)rand()/(float)RAND_MAX;

        Particle q;
        q.x = fr  * 2.0f - 1.0f;
        q.y = fr2 * 2.0f - 1.0f;
        q.z = fr3 * 2.0f - 1.0f;
        q.w = fr4 * 2.0f - 1.0f;
        q.vx = fv1 * 1.4f - 0.7f;
        q.vy = fv2 * 1.4f - 0.7f;
        q.vz = fv3 * 1.4f - 0.7f;
        q.vw = fv4 * 1.4f - 0.7f;
        q.hue = (float)rand()/(float)RAND_MAX;
        q.radius = 1.5f + 1.5f * ((float)rand()/(float)RAND_MAX);
        p[i] = q;
    }
}

static inline void project_4d_to_2d(float x, float y, float z, float w,
                                    float focal4, float focal3, int W, int H,
                                    float* outx, float* outy, float* outscale)
{
    float scale4 = focal4 / (focal4 - w);
    float X3 = x * scale4;
    float Y3 = y * scale4;
    float Z3 = z * scale4;

    float scale3 = focal3 / (focal3 - Z3);
    float X2 = X3 * scale3;
    float Y2 = Y3 * scale3;

    float cx = W * 0.5f;
    float cy = H * 0.5f;
    *outx = cx + X2 * cx * 0.9f;
    *outy = cy + Y2 * cy * 0.9f;
    *outscale = scale3 * scale4;
}

void update_particles(float dt, Particle* p, DrawItem* out, int N,
                      int W, int H, float t, float focal4, float focal3)
{
    float axw = 0.6f * t;
    float ayz = 0.4f * t;
    float sinxw = sinf(axw), cosxw = cosf(axw);
    float sinyz = sinf(ayz), cosyz = cosf(ayz);

    const float bounds = 1.15f;
    const float damp   = 0.85f;
    const float accel  = 0.20f;

    #pragma omp parallel for if(N>512) schedule(static)
    for (int i = 0; i < N; ++i) {
        Particle q = p[i];

        q.vx += accel * sinf(t*0.7f + q.y) * dt;
        q.vy += accel * cosf(t*0.5f + q.z) * dt;
        q.vz += accel * sinf(t*0.9f + q.w) * dt;
        q.vw += accel * cosf(t*0.8f + q.x) * dt;

        q.x += q.vx * dt;  q.y += q.vy * dt;
        q.z += q.vz * dt;  q.w += q.vw * dt;

        if (q.x < -bounds) { q.x = -bounds; q.vx = -q.vx * damp; }
        if (q.x >  bounds) { q.x =  bounds; q.vx = -q.vx * damp; }
        if (q.y < -bounds) { q.y = -bounds; q.vy = -q.vy * damp; }
        if (q.y >  bounds) { q.y =  bounds; q.vy = -q.vy * damp; }
        if (q.z < -bounds) { q.z = -bounds; q.vz = -q.vz * damp; }
        if (q.z >  bounds) { q.z =  bounds; q.vz = -q.vz * damp; }
        if (q.w < -bounds) { q.w = -bounds; q.vw = -q.vw * damp; }
        if (q.w >  bounds) { q.w =  bounds; q.vw = -q.vw * damp; }

        float x =  q.x * cosxw - q.w * sinxw;
        float w =  q.x * sinxw + q.w * cosxw;
        float y =  q.y * cosyz - q.z * sinyz;
        float z =  q.y * sinyz + q.z * cosyz;

        float px, py, scale;
        project_4d_to_2d(x, y, z, w, focal4, focal3, W, H, &px, &py, &scale);

        float hue = q.hue + 0.1f * w + 0.05f * z;
        uint8_t R,G,B;
        hsv_to_rgb(hue, 0.8f, 0.95f, &R, &G, &B);

        float rr = q.radius * (0.6f + 1.6f * clampf(scale, 0.2f, 2.5f));
        if (rr < 1.0f) rr = 1.0f;
        if (rr > 18.0f) rr = 18.0f;

        DrawItem di;
        di.x = px; di.y = py; di.r = rr;
        di.r8 = R; di.g8 = G; di.b8 = B; di.a8 = 210;
        out[i] = di;

        p[i] = q;
    }
}
