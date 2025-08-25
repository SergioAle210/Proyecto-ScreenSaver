#pragma once
#include <SDL2/SDL.h>

typedef struct {
    float px, py, pz;   // posición de cámara en mundo
    float yaw, pitch;   // orientación (rad) — yaw=Y, pitch=X
    float fov_deg;      // campo de visión vertical (grados)
} Camera3;

// Dibuja hipercubo 4D (tesseracto) proyectado a 2D con cámara 3D
void render_tesseract(SDL_Renderer* renderer, int W, int H,
                      float t, float focal4, const Camera3* cam);
