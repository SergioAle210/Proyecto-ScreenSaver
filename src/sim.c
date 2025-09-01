// sim.c — update de partículas optimizado con OpenMP + hints de vectorización
#include "sim.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h> // uint8_t
#include <string.h> // memset

#ifdef _OPENMP
#include <omp.h>
#endif

// --- utilidades inline y hints ------------------------------------------------

#ifndef LIKELY
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

static inline float clampf_fast(float v, float lo, float hi)
{
    // versión branchless usando fminf/fmaxf
    return fminf(fmaxf(v, lo), hi);
}

static inline void hsv_to_rgb_inline(float h, float s, float v,
                                     uint8_t *R, uint8_t *G, uint8_t *B)
{
    // Normaliza h a [0,1)
    h = h - floorf(h);
    float hf = h * 6.0f;
    float i = floorf(hf);
    float f = hf - i;

    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    float r, g, b;
    switch (((int)i) % 6)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break;
    }

    // Clamp + conversión a 0..255
    r = clampf_fast(r, 0.f, 1.f);
    g = clampf_fast(g, 0.f, 1.f);
    b = clampf_fast(b, 0.f, 1.f);
    *R = (uint8_t)(r * 255.0f);
    *G = (uint8_t)(g * 255.0f);
    *B = (uint8_t)(b * 255.0f);
}

static inline void project_4d_to_2d_inline(float x, float y, float z, float w,
                                           float focal4, float focal3, int W, int H,
                                           float *outx, float *outy, float *outscale)
{
    // proyección 4D->3D, luego 3D->2D, con centro y escala a pantalla
    float scale4 = focal4 / (focal4 - w);
    float X3 = x * scale4;
    float Y3 = y * scale4;
    float Z3 = z * scale4;

    float scale3 = focal3 / (focal3 - Z3);
    float X2 = X3 * scale3;
    float Y2 = Y3 * scale3;

    float cx = W * 0.5f, cy = H * 0.5f;
    *outx = cx + X2 * cx * 0.9f;
    *outy = cy + Y2 * cy * 0.9f;
    *outscale = scale3 * scale4;
}

// --- API pública --------------------------------------------------------------

void init_particles(Particle *p, int N, unsigned int seed)
{
    // Init secuencial (se llama una sola vez).
    // Dejamos rand() aquí porque no hay paralelismo ni carreras.
    srand(seed);
    for (int i = 0; i < N; ++i)
    {
        float fr1 = (float)rand() / (float)RAND_MAX;
        float fr2 = (float)rand() / (float)RAND_MAX;
        float fr3 = (float)rand() / (float)RAND_MAX;
        float fr4 = (float)rand() / (float)RAND_MAX;
        float fv1 = (float)rand() / (float)RAND_MAX;
        float fv2 = (float)rand() / (float)RAND_MAX;
        float fv3 = (float)rand() / (float)RAND_MAX;
        float fv4 = (float)rand() / (float)RAND_MAX;

        Particle q;
        q.x = fr1 * 2.0f - 1.0f;
        q.y = fr2 * 2.0f - 1.0f;
        q.z = fr3 * 2.0f - 1.0f;
        q.w = fr4 * 2.0f - 1.0f;

        q.vx = fv1 * 1.4f - 0.7f;
        q.vy = fv2 * 1.4f - 0.7f;
        q.vz = fv3 * 1.4f - 0.7f;
        q.vw = fv4 * 1.4f - 0.7f;

        q.hue = (float)rand() / (float)RAND_MAX;
        q.radius = 1.5f + 1.5f * ((float)rand() / (float)RAND_MAX);

        p[i] = q;
    }
}

void update_particles(float dt,
                      Particle *__restrict p,
                      DrawItem *__restrict out,
                      int N, int W, int H,
                      float t, float focal4, float focal3)
{
    // Pre-cálculos compartidos (baratos, fuera del bucle)
    const float axw = 0.6f * t;
    const float ayz = 0.4f * t;

    // Si tu libm tiene sincosf, puedes activarlo con -ffast-math (GNU):
    // float sinxw, cosxw; sincosf(axw, &sinxw, &cosxw);
    // float sinyz, cosy z; sincosf(ayz, &sinyz, &cosyz);
    // Para compatibilidad general:
    const float sinxw = sinf(axw), cosxw = cosf(axw);
    const float sinyz = sinf(ayz), cosyz = cosf(ayz);

    const float bounds = 1.15f;
    const float damp = 0.85f;
    const float accel = 0.20f;

    // ---------------------------------------------
    // Bucle principal: 1 iteración = 1 partícula
    // Paralelizamos si N es suficientemente grande
    // ---------------------------------------------
    // Hints para el compilador: iteraciones independientes & vectorizables
    // (la escritura es out[i] y p[i]; cada i es disjunto)
#ifdef _OPENMP
#pragma omp parallel for if (N > 1024) schedule(static)
#endif
    for (int i = 0; i < N; ++i)
    {

        // Opcional: prefetch (no siempre ayuda, depende de CPU)
        // __builtin_prefetch(&p[i+16], 0, 0);
        // __builtin_prefetch(&out[i+16], 1, 0);

        Particle q = p[i];

        // Integración con pequeñas fuerzas pseudo-periódicas
        // (usar dt fuera de trig para mejor vectorización)
        float s1 = sinf(t * 0.7f + q.y);
        float c1 = cosf(t * 0.5f + q.z);
        float s2 = sinf(t * 0.9f + q.w);
        float c2 = cosf(t * 0.8f + q.x);

        q.vx += accel * s1 * dt;
        q.vy += accel * c1 * dt;
        q.vz += accel * s2 * dt;
        q.vw += accel * c2 * dt;

        q.x += q.vx * dt;
        q.y += q.vy * dt;
        q.z += q.vz * dt;
        q.w += q.vw * dt;

        // Rebotes contra hipercubo [-bounds, +bounds]
        if (UNLIKELY(q.x < -bounds))
        {
            q.x = -bounds;
            q.vx = -q.vx * damp;
        }
        else if (UNLIKELY(q.x > bounds))
        {
            q.x = bounds;
            q.vx = -q.vx * damp;
        }

        if (UNLIKELY(q.y < -bounds))
        {
            q.y = -bounds;
            q.vy = -q.vy * damp;
        }
        else if (UNLIKELY(q.y > bounds))
        {
            q.y = bounds;
            q.vy = -q.vy * damp;
        }

        if (UNLIKELY(q.z < -bounds))
        {
            q.z = -bounds;
            q.vz = -q.vz * damp;
        }
        else if (UNLIKELY(q.z > bounds))
        {
            q.z = bounds;
            q.vz = -q.vz * damp;
        }

        if (UNLIKELY(q.w < -bounds))
        {
            q.w = -bounds;
            q.vw = -q.vw * damp;
        }
        else if (UNLIKELY(q.w > bounds))
        {
            q.w = bounds;
            q.vw = -q.vw * damp;
        }

        // Rotaciones (X<->W) y (Y<->Z)
        float x = q.x * cosxw - q.w * sinxw;
        float w = q.x * sinxw + q.w * cosxw;
        float y = q.y * cosyz - q.z * sinyz;
        float z = q.y * sinyz + q.z * cosyz;

        // Proyección 4D->2D
        float px, py, scale;
        project_4d_to_2d_inline(x, y, z, w, focal4, focal3, W, H, &px, &py, &scale);

        // Color (HSV) animado con w,z
        float hue = q.hue + 0.1f * w + 0.05f * z;
        uint8_t R, G, B;
        hsv_to_rgb_inline(hue, 0.8f, 0.95f, &R, &G, &B);

        // Radio derivado de la escala de proyección (clamp branchless)
        float rr = q.radius * (0.6f + 1.6f * clampf_fast(scale, 0.2f, 2.5f));
        rr = clampf_fast(rr, 1.0f, 18.0f);

        // Emitir DrawItem (un write por elemento; sin colisiones)
        DrawItem di;
        di.x = px;
        di.y = py;
        di.r = rr;
        di.r8 = R;
        di.g8 = G;
        di.b8 = B;
        di.a8 = 210;

        out[i] = di;
        p[i] = q;
    }
}
