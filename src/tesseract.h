#pragma once
#include <SDL2/SDL.h>

// Dibuja un hipercubo 4D (tesseracto) en wireframe, proyectado a 2D.
void render_tesseract(SDL_Renderer* renderer, int W, int H, float t, float focal4, float focal3);
