#pragma once
#include <SDL2/SDL.h>

typedef struct {
    float px, py, pz;   // posición de cámara
    float yaw, pitch;   // orientación (rad)
    float fov_deg;      // FOV vertical (grados)
} Camera3;

typedef enum {
    TESS_ROTATE = 0,    // (por compatibilidad) rotando en 4D
    TESS_STATIC = 1,    // fijo, sin rotación 4D
    TESS_EDGES  = 2,    // aristas hechas de "partículas" pulsantes
    TESS_GALAXY = 3     // muchos tesseractos
} TessSubmode;

// Renderizador principal de tesseracto con submodo
void render_tesseract_mode(SDL_Renderer* renderer, int W, int H,
                           float t, float focal4, const Camera3* cam,
                           TessSubmode submode);
