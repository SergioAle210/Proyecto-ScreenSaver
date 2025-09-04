#ifndef CLOTH_H
#define CLOTH_H

#include <SDL2/SDL.h>
#include "sim.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        int GX, GY;         // Grid (cols x rows). Si 0, se deriva de N/aspecto
        float spanX, spanY; // Tamaño “mundo”
        float tiltX_deg;    // Inclinación X (grados)
        float tiltY_deg;    // Inclinación Y (grados)
        float zCam;         // Cámara (z)
        float fov;          // Campo de visión
        float baseRadius;   // Radio base (px); si <=0 se ajusta automático
        float amp;          // Amplitud de la onda
        float sigma;        // Dispersión gaussiana
        float omega;        // Frecuencia global (rad/s)
        float speed;        // Velocidad en XY
        float colorSpeed;   // Velocidad de cambio de color (hue)
        float panX_px;      // Paneo X (px)
        float panY_px;      // Paneo Y (px)
        int autoCenter;     // 1 = centrar automáticamente (default)
    } ClothParams;

    typedef struct
    {
        ClothParams P;
        int W_last, H_last;
        // Número total de partículas
        int N;

        // Arreglo de elementos para dibujar
        DrawItem *draw;
        // Arreglo de profundidades
        float *depth;
        // Orden final y capacidad reservada
        int *order_idx;
        int order_cap;

        // Sprite circular (textura) y radio en px
        SDL_Texture *sprite;
        int spriteRadius;

        float tx, ty; // offset de paneo/centrado (suavizado)
    } ClothState;

    // Inicialización de la tela
    int cloth_init(SDL_Renderer *R, ClothState *S, const ClothParams *P_in, int W, int H);
    // Calcula posiciones proyecta
    void cloth_update(SDL_Renderer *R, ClothState *S, int W, int H, float t);
    // Liberación de recursos
    void cloth_destroy(ClothState *S);

    // Backends de dibujo
    void cloth_render_seq(SDL_Renderer *R, const ClothState *S);
    // Prepara la geometria en paralelo y si falla se usa secuencial
    void cloth_render_omp(SDL_Renderer *R, const ClothState *S);
    // Liberación de recursos
    void cloth_draw_omp_release(void);

#ifdef __cplusplus
}
#endif
#endif
