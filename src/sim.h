// sim.h
#ifndef SIM_H
#define SIM_H

#include <stdint.h>

/**
 * Estructura de partícula en 4D
 */
typedef struct
{
    float x, y, z, w;     // posición
    float vx, vy, vz, vw; // velocidad
    float hue;            // tono de color
    float radius;         // radio de render
} Particle;

/**
 * Estructura de dibujo lista para enviar a SDL2
 */
typedef struct
{
    float x, y;             // coordenadas proyectadas
    float r;                // radio proyectado
    uint8_t r8, g8, b8, a8; // color RGBA
} DrawItem;

// Inicializa N partículas con posiciones aleatorias
void init_particles(Particle *p, int N, unsigned int seed);

// Actualiza las partículas (versión secuencial y paralela con OpenMP)
void update_particles(float dt, Particle *p, DrawItem *out, int N,
                      int W, int H, float t, float focal4, float focal3);

#endif
