#include "tesseract.h"
#include <math.h>
#include <stdint.h>

typedef struct { float x,y,z,w; } V4;
typedef struct { float x,y; } V2;

static inline float clampf(float v, float lo, float hi) { return v<lo?lo:(v>hi?hi:v); }

void render_tesseract(SDL_Renderer* R, int W, int H, float t, float focal4, float focal3)
{
    // 16 vértices del hipercubo en [-1,1]^4
    const float s = 0.9f;
    V4 v4[16];
    for (int i=0;i<16;++i){
        v4[i].x = ((i & 1) ? +s : -s);
        v4[i].y = ((i & 2) ? +s : -s);
        v4[i].z = ((i & 4) ? +s : -s);
        v4[i].w = ((i & 8) ? +s : -s);
    }

    // Ángulos de rotación 4D (varían con el tiempo)
    const float axw = 0.6f * t;
    const float ayz = 0.4f * t;
    const float axy = 0.2f * t;
    const float azw = 0.3f * t;

    const float sxw = sinf(axw), cxw = cosf(axw);
    const float syz = sinf(ayz), cyz = cosf(ayz);
    const float sxy = sinf(axy), cxy = cosf(axy);
    const float szw = sinf(azw), bzw = cosf(azw);

    // Proyectar 4D->3D->2D
    V2 p2[16];
    float depth[16];
    const float cx = W * 0.5f, cy = H * 0.5f;

    for (int i=0;i<16;++i){
        float x=v4[i].x, y=v4[i].y, z=v4[i].z, w=v4[i].w;

        // Rotaciones 4D: x<->w, y<->z, x<->y, z<->w
        float x1 =  x*cxw - w*sxw;
        float w1 =  x*sxw + w*cxw;
        float y1 =  y*cyz - z*syz;
        float z1 =  y*syz + z*cyz;

        float x2 =  x1*cxy - y1*sxy;
        float y2 =  x1*sxy + y1*cxy;
        float z2 =  z1*bzw - w1*szw;
        float w2 =  z1*szw + w1*bzw;

        // 4D->3D (perspectiva en w)
        float scale4 = focal4 / (focal4 - w2);
        float X3 = x2 * scale4;
        float Y3 = y2 * scale4;
        float Z3 = z2 * scale4;

        // 3D->2D (perspectiva en z)
        float scale3 = focal3 / (focal3 - Z3);
        float X2 = X3 * scale3;
        float Y2 = Y3 * scale3;

        // A pixeles
        p2[i].x = cx + X2 * cx * 0.75f;
        p2[i].y = cy + Y2 * cy * 0.75f;

        // Profundidad para sombreado simple
        depth[i] = clampf((scale3*scale4 - 0.5f) * 0.8f, 0.0f, 1.0f);
    }

    // Dibujar aristas: vértices que difieren en 1 bit
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    for (int i=0;i<16;++i){
        for (int d=0; d<4; ++d){
            int j = i ^ (1<<d);
            if (j > i){
                float a = 0.65f + 0.35f * (depth[i] + depth[j]) * 0.5f;
                uint8_t c = (uint8_t)(clampf(a,0.f,1.f) * 255.0f);
                SDL_SetRenderDrawColor(R, c, c, c, 230);
                SDL_RenderDrawLineF(R, p2[i].x, p2[i].y, p2[j].x, p2[j].y);
            }
        }
    }
}
