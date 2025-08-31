// cloth.h — Manta de esferas 3D con "aguja" y color pulsante (OpenMP + SDL2)
#ifndef CLOTH_H
#define CLOTH_H

#include <SDL2/SDL.h>
#include "sim.h"   // Para reutilizar DrawItem (x,y,r, rgba)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   GX, GY;        // Grid (columnas x filas). Si 0, se calcula desde N y aspecto ventana
    float spanX, spanY;  // Tamaño en “mundo” de la manta (en unidades normalizadas)
    float tiltX_deg;     // Inclinación X (grados) para efecto 3D
    float tiltY_deg;     // Inclinación Y (grados) opcional (yaw ligero)
    float zCam;          // Cámara (z)
    float fov;           // Campo de visión (perspectiva). ~[0.8..1.4]
    float baseRadius;    // Radio base (px); si 0, se deriva de W/GX y H/GY
    float amp;           // Amplitud de la ondulación “aguja”
    float sigma;         // Dispersión de la aguja (Gaussian)
    float omega;         // Frecuencia global de palpitar (rad/s)
    float speed;         // Velocidad de desplazamiento de la aguja en XY
    float colorSpeed;    // Velocidad del ciclo de color (hue)
} ClothParams;

typedef struct {
    ClothParams P;
    int W_last, H_last;
    int N;               // GX*GY
    DrawItem* draw;      // buffer de dibujo (pos/color/radio)
    float*    depth;     // buffer de profundidad para ordenar
    SDL_Texture* sprite; // sprite circular con alfa (para “bolas” suaves)
    int spriteRadius;    // radio del sprite base
} ClothState;

// Inicializa el estado (crea sprite, buffers, etc.)
int cloth_init(SDL_Renderer* R, ClothState* S, const ClothParams* P_in, int W, int H);

// Actualiza la manta y la renderiza (ordena por profundidad y dibuja)
void cloth_update_and_render(SDL_Renderer* R, ClothState* S, int W, int H, float t);

// Libera recursos
void cloth_destroy(ClothState* S);

#ifdef __cplusplus
}
#endif
#endif
