// cube3d.c — Wireframe cube: micro-opt + robustez numérica
#include "cube3d.h"
#include <math.h>
#include <stdint.h>

// Estructuras locales
typedef struct
{
    float x, y, z;
} Vec3;
typedef struct
{
    float x, y;
} Vec2;

// Geometría base
static const Vec3 CUBE_VERTS[8] = {
    {-1.f, -1.f, -1.f}, {+1.f, -1.f, -1.f}, {+1.f, +1.f, -1.f}, {-1.f, +1.f, -1.f}, {-1.f, -1.f, +1.f}, {+1.f, -1.f, +1.f}, {+1.f, +1.f, +1.f}, {-1.f, +1.f, +1.f}};
static const int CUBE_EDGES[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

// Rotación Y seguida de X, con senos/cosenos ya precalculados
static inline Vec3 rotate_yx(const Vec3 v, float sy, float cy, float sx, float cx)
{
    // Y
    float x1 = v.x * cy - v.z * sy;
    float z1 = v.x * sy + v.z * cy;
    float y1 = v.y;
    // X
    Vec3 r;
    r.x = x1;
    r.y = y1 * cx - z1 * sx;
    r.z = y1 * sx + z1 * cx;
    return r;
}

static inline Vec2 project_3d_to_2d(const Vec3 v, int W, int H, float fov, float z_cam)
{
    // Evita división por ~0 (cuando el punto queda muy cerca del plano de cámara)
    float denom = (v.z - z_cam);
    if (fabsf(denom) < 1e-4f)
        denom = (denom >= 0.f ? 1e-4f : -1e-4f);

    float scale = fov / denom;
    float hw = 0.5f * (float)W, hh = 0.5f * (float)H;
    Vec2 out = {v.x * scale * hw + hw, v.y * scale * hh + hh};
    return out;
}

void render_cube3d(SDL_Renderer *R, int W, int H, float time)
{
    // Parámetros (podrías exponerlos por CLI si quieres tunear el look)
    const float fov = 1.0f;
    const float z_cam = -5.0f;
    const float z_push = 4.0f; // empuja el cubo hacia delante (delante de la cámara)

    // Precalcula senos/cosenos una sola vez
    float angY = time;
    float angX = time * 0.5f;
    float sy = sinf(angY), cy = cosf(angY);
    float sx = sinf(angX), cx = cosf(angX);

    // Proyecta los 8 vértices transformados
    Vec2 proj[8];
    for (int i = 0; i < 8; ++i)
    {
        Vec3 v = CUBE_VERTS[i];
        v = rotate_yx(v, sy, cy, sx, cx);
        v.z += z_push;
        proj[i] = project_3d_to_2d(v, W, H, fov, z_cam);
    }

    // Dibuja las 12 aristas (wireframe)
    SDL_SetRenderDrawColor(R, 255, 255, 255, 255);
    for (int i = 0; i < 12; ++i)
    {
        int a = CUBE_EDGES[i][0], b = CUBE_EDGES[i][1];
        SDL_RenderDrawLineF(R, proj[a].x, proj[a].y, proj[b].x, proj[b].y);
    }
}
