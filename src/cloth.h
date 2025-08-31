// cloth.h — Manta de esferas 3D con "aguja" y color pulsante (OpenMP + SDL2)
#ifndef CLOTH_H
#define CLOTH_H

#include <SDL2/SDL.h>
#include "sim.h"   // Define DrawItem

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   GX, GY;        // Grid (cols x rows). Si 0, se deriva de N/aspecto
    float spanX, spanY;  // Tamaño en “mundo” de la manta
    float tiltX_deg;     // Inclinación X (grados)
    float tiltY_deg;     // Inclinación Y (grados)
    float zCam;          // Cámara (z)
    float fov;           // Campo de visión (perspectiva)
    float baseRadius;    // Radio base (px). Si >0, se respeta (override)
    float amp;           // Amplitud de la onda-aguja
    float sigma;         // Dispersión gaussiana de la aguja
    float omega;         // Frecuencia global de palpitar (rad/s)
    float speed;         // Velocidad de la aguja en XY
    float colorSpeed;    // Velocidad del ciclo de color (hue)

    // Centrado/paneo en pantalla
    float panX_px;       // + derecha / - izquierda
    float panY_px;       // + abajo   / - arriba
    int   autoCenter;    // 1 = centrar automáticamente (default)
} ClothParams;

typedef struct {
    ClothParams P;
    int W_last, H_last;
    int N;
    DrawItem* draw;      // buffer de dibujo
    float*    depth;     // buffer de profundidad
    SDL_Texture* sprite; // sprite circular con alfa
    int spriteRadius;

    // Offset suavizado para centrar/panear
    float tx, ty;
} ClothState;

int  cloth_init(SDL_Renderer* R, ClothState* S, const ClothParams* P_in, int W, int H);
void cloth_update_and_render(SDL_Renderer* R, ClothState* S, int W, int H, float t);
void cloth_destroy(ClothState* S);

#ifdef __cplusplus
}
#endif
#endif
