// cube3d.h
#ifndef CUBE3D_H
#define CUBE3D_H

#include <SDL2/SDL.h>

/**
 * Renderiza un cubo 3D girando en perspectiva.
 * @param renderer Puntero al renderer SDL2.
 * @param width Ancho de ventana.
 * @param height Alto de ventana.
 * @param time Tiempo actual en segundos (para animación).
 */
void render_cube3d(SDL_Renderer* renderer, int width, int height, float time);

#endif
