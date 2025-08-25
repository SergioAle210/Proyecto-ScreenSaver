#include "tesseract.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { float x,y,z,w; } V4;

static inline float clampf(float v, float lo, float hi) { return v<lo?lo:(v>hi?hi:v); }

/* -------------------- util proyección 4D->3D->2D con cámara 3D -------------------- */
static int project_vertex(const V4 v, float axw, float ayz, float axy, float azw,
                          float focal4, const Camera3* cam, int W, int H,
                          float* out_x, float* out_y, float* out_zcam, float* out_brightness)
{
    /* Rotaciones 4D (si los ángulos son 0 -> queda fijo) */
    const float sxw = sinf(axw), cxw = cosf(axw);
    const float syz = sinf(ayz), cyz = cosf(ayz);
    const float sxy = sinf(axy), cxy = cosf(axy);
    const float szw = sinf(azw), bzw = cosf(azw);

    float x=v.x, y=v.y, z=v.z, w=v.w;

    float x1 =  x*cxw - w*sxw;
    float w1 =  x*sxw + w*cxw;
    float y1 =  y*cyz - z*syz;
    float z1 =  y*syz + z*cyz;

    float x2 =  x1*cxy - y1*sxy;
    float y2 =  x1*sxy + y1*cxy;
    float z2 =  z1*bzw - w1*szw;
    float w2 =  z1*szw + w1*bzw;

    /* 4D -> 3D (perspectiva por w) */
    float scale4 = focal4 / (focal4 - w2);
    float X3 = x2 * scale4;
    float Y3 = y2 * scale4;
    float Z3 = z2 * scale4;

    /* Vista de cámara */
    float Xw = X3 - cam->px, Yw = Y3 - cam->py, Zw = Z3 - cam->pz;
    float cyaw = cosf(cam->yaw), syaw = sinf(cam->yaw);
    float cpit = cosf(cam->pitch), spit = sinf(cam->pitch);
    float xr =  cyaw*Xw - syaw*Zw;
    float zr =  syaw*Xw + cyaw*Zw;
    float yr =  cpit*Yw - spit*zr;
    float zz =  spit*Yw + cpit*zr;           /* profundidad en cámara */

    /* 3D -> 2D */
    if (zz <= 0.001f) return 0;               /* detrás de la cámara */
    const float f = 1.0f / tanf((cam->fov_deg*(float)M_PI/180.0f)*0.5f);
    float scale3 = f / zz;
    float X2 = xr * scale3, Y2 = yr * scale3;

    float cx = W * 0.5f, cy = H * 0.5f;
    *out_x = cx + X2 * cx;
    *out_y = cy + Y2 * cy;
    *out_zcam = zz;
    *out_brightness = clampf(1.0f - (zz*0.08f), 0.0f, 1.0f); /* cerca -> brillante */
    return 1;
}

/* -------------------- núcleo: genera 16 vértices y 32 aristas -------------------- */
static void build_tesseract(V4 out[16], float size)
{
    for (int i=0;i<16;++i){
        out[i].x = ((i & 1) ? +size : -size);
        out[i].y = ((i & 2) ? +size : -size);
        out[i].z = ((i & 4) ? +size : -size);
        out[i].w = ((i & 8) ? +size : -size);
    }
}

static void draw_edges_lines(SDL_Renderer* R, int W, int H,
                             float* X, float* Y, float* Zcam, float* B)
{
    (void)W; (void)H;
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    for (int i=0;i<16;++i){
        for (int d=0; d<4; ++d){
            int j = i ^ (1<<d);
            if (j > i){
                if (Zcam[i]<=0.001f && Zcam[j]<=0.001f) continue;
                float a = 0.55f + 0.45f * (B[i]+B[j])*0.5f;
                uint8_t c = (uint8_t)(clampf(a,0.f,1.f)*255.0f);
                SDL_SetRenderDrawColor(R, c, c, c, 235);
                SDL_RenderDrawLineF(R, X[i], Y[i], X[j], Y[j]);
            }
        }
    }
}

static void draw_edges_particles(SDL_Renderer* R,
                                 float* X, float* Y, float* Zcam, float* B,
                                 float t)
{
    /* muestreamos partículas a lo largo de cada arista (neón pulsante) */
    const int SAMPLES = 42;
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    for (int i=0;i<16;++i){
        for (int d=0; d<4; ++d){
            int j = i ^ (1<<d);
            if (j > i){
                if (Zcam[i]<=0.001f && Zcam[j]<=0.001f) continue;
                float x0=X[i], y0=Y[i], x1=X[j], y1=Y[j];
                for (int k=0;k<SAMPLES;++k){
                    float u = (float)k/(float)(SAMPLES-1);
                    float x = x0 + (x1-x0)*u;
                    float y = y0 + (y1-y0)*u;

                    /* pulso en capas: dos senos desfasados */
                    float pulse = 0.6f + 0.4f*sinf(10.0f*u + t*2.2f) * cosf(6.0f*u + t*1.3f);
                    float b = clampf((B[i]*(1.0f-u) + B[j]*u)*0.8f + 0.2f*pulse, 0.0f, 1.0f);

                    /* halo + núcleo */
                    uint8_t core = (uint8_t)(clampf(0.75f + 0.25f*b,0,1)*255);
                    uint8_t halo = (uint8_t)(clampf(0.35f + 0.65f*b,0,1)*255);

                    float r_core = 1.2f + 2.2f*pulse;
                    float r_halo = r_core*2.2f;

                    SDL_SetRenderDrawColor(R, halo, (uint8_t)(halo*0.6f), 255, 90);
                    SDL_FRect hrect = { x - r_halo*0.5f, y - r_halo*0.5f, r_halo, r_halo };
                    SDL_RenderFillRectF(R, &hrect);

                    SDL_SetRenderDrawColor(R, 255, (uint8_t)(core*0.4f), core, 220);
                    SDL_FRect crect = { x - r_core*0.5f, y - r_core*0.5f, r_core, r_core };
                    SDL_RenderFillRectF(R, &crect);
                }
            }
        }
    }
}

/* -------------------- “galaxia” de muchos tesseractos -------------------- */
typedef struct { float cx,cy,cz,cw, scale; } TessInstance;

static uint32_t lcg(uint32_t* s){ *s = (*s)*1664525u + 1013904223u; return *s; }

/* Tamaño del arreglo DEBE ser constante en compilación */
#define TESS_GALAXY_COUNT 40
static int g_inited = 0;
static TessInstance g_inst[TESS_GALAXY_COUNT];

static void render_galaxy(SDL_Renderer* R, int W, int H, float t,
                          float focal4, const Camera3* cam)
{
    const int COUNT = TESS_GALAXY_COUNT;
    if (!g_inited){
        uint32_t seed = 123456u;
        for (int i=0;i<COUNT;++i){
            float rx = ((int)(lcg(&seed)%2000)-1000)/160.0f; /* aprox -6..+6 */
            float ry = ((int)(lcg(&seed)%2000)-1000)/160.0f;
            float rz = ((int)(lcg(&seed)%2000)-1000)/160.0f;
            float rw = ((int)(lcg(&seed)%2000)-1000)/160.0f;
            float sc = 0.4f + (lcg(&seed)%1000)/1000.0f * 1.4f;
            g_inst[i] = (TessInstance){rx,ry,rz,rw, sc};
        }
        g_inited = 1;
    }

    const float size = 0.9f;
    V4 base[16]; build_tesseract(base, size);

    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    for (int n=0;n<COUNT;++n){
        /* sin rotación 4D para “fijo”; leve respiración en w para sensación 4D */
        float axw=0, ayz=0, axy=0, azw=0;
        float X[16],Y[16],Z[16],B[16];

        for (int i=0;i<16;++i){
            V4 v = base[i];
            v.x = v.x*g_inst[n].scale + g_inst[n].cx;
            v.y = v.y*g_inst[n].scale + g_inst[n].cy;
            v.z = v.z*g_inst[n].scale + g_inst[n].cz;
            v.w = v.w*g_inst[n].scale + g_inst[n].cw + 0.2f*sinf(t*0.6f + 0.7f*g_inst[n].cx); /* “parpadeo” 4D */
            if (!project_vertex(v, axw,ayz,axy,azw, focal4, cam, W,H, &X[i],&Y[i],&Z[i],&B[i])) {
                X[i]=Y[i]=Z[i]=B[i]=0;
            }
        }

        /* aristas finas + vértices brillantes */
        for (int i=0;i<16;++i){
            for (int d=0; d<4; ++d){
                int j = i ^ (1<<d);
                if (j > i){
                    if (Z[i]<=0.001f && Z[j]<=0.001f) continue;
                    float a = 0.35f + 0.65f*(B[i]+B[j])*0.5f;
                    SDL_SetRenderDrawColor(R, (uint8_t)(255*a), (uint8_t)(90*a), (uint8_t)(255*a), 130);
                    SDL_RenderDrawLineF(R, X[i],Y[i], X[j],Y[j]);
                }
            }
            /* “estrella” en vértice */
            float r = 1.5f + 2.0f*B[i];
            SDL_SetRenderDrawColor(R, 255, 90, 255, 200);
            SDL_FRect rc = { X[i]-r*0.5f, Y[i]-r*0.5f, r, r };
            SDL_RenderFillRectF(R, &rc);
        }
    }
}

/* -------------------- entrada única para todos los submodos -------------------- */
void render_tesseract_mode(SDL_Renderer* R, int W, int H, float t,
                           float focal4, const Camera3* cam, TessSubmode sub)
{
    if (sub == TESS_GALAXY){
        render_galaxy(R, W, H, t, focal4, cam);
        return;
    }

    /* construir vértices de un tesseracto */
    const float size = 1.2f;
    V4 verts[16]; build_tesseract(verts, size);

    /* elegir ángulos (rotando o fijos) */
    float axw=0, ayz=0, axy=0, azw=0;
    if (sub == TESS_ROTATE){
        axw = 0.6f*t; ayz = 0.4f*t; axy = 0.2f*t; azw = 0.3f*t;
    } /* STATIC/EDGES -> 0 */

    /* proyectar */
    float X[16],Y[16],Z[16],B[16];
    for (int i=0;i<16;++i){
        if (!project_vertex(verts[i], axw,ayz,axy,azw,
                            focal4, cam, W,H, &X[i],&Y[i],&Z[i],&B[i])){
            X[i]=Y[i]=Z[i]=B[i]=0;
        }
    }

    if (sub == TESS_EDGES) {
        draw_edges_particles(R, X,Y,Z,B, t);
    } else {
        draw_edges_lines(R, W,H, X,Y,Z,B);
    }
}
