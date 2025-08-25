#include "tesseract.h"
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { float x,y,z,w; } V4;
typedef struct { float x,y; } V2;

static inline float clampf(float v, float lo, float hi) { return v<lo?lo:(v>hi?hi:v); }

void render_tesseract(SDL_Renderer* R, int W, int H, float t, float focal4, const Camera3* cam)
{
    // 16 vértices del hipercubo en [-1,1]^4, algo grande
    const float s = 1.2f;
    V4 v4[16];
    for (int i=0;i<16;++i){
        v4[i].x = ((i & 1) ? +s : -s);
        v4[i].y = ((i & 2) ? +s : -s);
        v4[i].z = ((i & 4) ? +s : -s);
        v4[i].w = ((i & 8) ? +s : -s);
    }

    // Rotaciones 4D (tiempo)
    const float axw = 0.6f * t;
    const float ayz = 0.4f * t;
    const float axy = 0.2f * t;
    const float azw = 0.3f * t;
    const float sxw = sinf(axw), cxw = cosf(axw);
    const float syz = sinf(ayz), cyz = cosf(ayz);
    const float sxy = sinf(axy), cxy = cosf(axy);
    const float szw = sinf(azw), bzw = cosf(azw);

    // Parámetros de cámara
    const float fov = cam->fov_deg * (float)M_PI / 180.0f;
    const float f = 1.0f / tanf(fov * 0.5f); // proyección perspectiva clásica
    const float cx = W * 0.5f, cy = H * 0.5f;

    // Proyectar cada vértice
    float x2d[16], y2d[16], zcam[16], depth[16];
    for (int i=0;i<16;++i){
        float x=v4[i].x, y=v4[i].y, z=v4[i].z, w=v4[i].w;

        // 4D: x<->w, y<->z, x<->y, z<->w
        float x1 =  x*cxw - w*sxw;
        float w1 =  x*sxw + w*cxw;
        float y1 =  y*cyz - z*syz;
        float z1 =  y*syz + z*cyz;

        float x2 =  x1*cxy - y1*sxy;
        float y2 =  x1*sxy + y1*cxy;
        float z2 =  z1*bzw - w1*szw;
        float w2 =  z1*szw + w1*bzw;

        // 4D → 3D (perspectiva en w)
        float scale4 = focal4 / (focal4 - w2);
        float X3 = x2 * scale4;
        float Y3 = y2 * scale4;
        float Z3 = z2 * scale4;

        // Cámara: trasladar y rotar a espacio de vista (yaw, luego pitch)
        float Xw = X3 - cam->px;
        float Yw = Y3 - cam->py;
        float Zw = Z3 - cam->pz;

        float cyaw = cosf(cam->yaw), syaw = sinf(cam->yaw);
        float cpit = cosf(cam->pitch), spit = sinf(cam->pitch);

        float xr =  cyaw*Xw - syaw*Zw;
        float zr =  syaw*Xw + cyaw*Zw;

        float yr =  cpit*Yw - spit*zr;
        float zz =  spit*Yw + cpit*zr;   // profundidad en cámara

        zcam[i] = zz;

        // 3D → 2D (perspectiva; descartamos si está detrás de la cámara)
        if (zz <= 0.001f) {
            x2d[i] = y2d[i] = 1e9f; // fuera de pantalla
            depth[i] = 0.f;
            continue;
        }
        float scale3 = f / zz;
        float X2 = xr * scale3;
        float Y2 = yr * scale3;

        x2d[i] = cx + X2 * cx;
        y2d[i] = cy + Y2 * cy;
        depth[i] = clampf(1.0f - (zz*0.08f), 0.0f, 1.0f); // más cerca → más brillante
    }

    // Dibujar aristas (vértices que difieren en un bit)
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    for (int i=0;i<16;++i){
        for (int d=0; d<4; ++d){
            int j = i ^ (1<<d);
            if (j > i){
                // Si ambos vértices están detrás de la cámara, omite
                if (zcam[i] <= 0.001f && zcam[j] <= 0.001f) continue;

                float a = 0.55f + 0.45f * (depth[i] + depth[j]) * 0.5f;
                uint8_t c = (uint8_t)(clampf(a,0.f,1.f) * 255.0f);
                SDL_SetRenderDrawColor(R, c, c, c, 235);
                SDL_RenderDrawLineF(R, x2d[i], y2d[i], x2d[j], y2d[j]);
            }
        }
    }
}
