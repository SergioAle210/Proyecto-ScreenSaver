#pragma once
#include <stdint.h>

typedef struct {
    float x, y, z, w;
    float vx, vy, vz, vw;
    float hue;
    float radius;
} Particle;

typedef struct {
    float x, y, r;   // posición en pantalla + tamaño
    uint8_t r8, g8, b8, a8; // color
} DrawItem;

void init_particles(Particle* p, int N, unsigned int seed);
void update_particles(float dt, Particle* p, DrawItem* out, int N,
                      int W, int H, float t, float focal4, float focal3);
