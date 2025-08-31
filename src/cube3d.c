// cube3d.c
#include "cube3d.h"
#include <math.h>
#include <stdint.h>

typedef struct
{
    float x, y, z;
} Vec3;
typedef struct
{
    float x, y;
} Vec2;

static Vec3 cube_vertices[8] = {
    {-1, -1, -1}, {+1, -1, -1}, {+1, +1, -1}, {-1, +1, -1}, {-1, -1, +1}, {+1, -1, +1}, {+1, +1, +1}, {-1, +1, +1}};

static int cube_edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

static Vec3 rotate_y(Vec3 v, float a)
{
    float c = cosf(a), s = sinf(a);
    return (Vec3){v.x * c - v.z * s, v.y, v.x * s + v.z * c};
}

static Vec3 rotate_x(Vec3 v, float a)
{
    float c = cosf(a), s = sinf(a);
    return (Vec3){v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}

static Vec2 project_3d_to_2d(Vec3 v, int W, int H, float fov, float z_cam)
{
    float scale = fov / (v.z - z_cam);
    return (Vec2){(v.x * scale * W / 2.0f) + W / 2.0f, (v.y * scale * H / 2.0f) + H / 2.0f};
}

void render_cube3d(SDL_Renderer *R, int W, int H, float time)
{
    Vec2 proj[8];
    float angle = time, fov = 1.0f, z_cam = -5.0f;
    for (int i = 0; i < 8; ++i)
    {
        Vec3 v = cube_vertices[i];
        v = rotate_y(v, angle);
        v = rotate_x(v, angle * 0.5f);
        v.z += 4.0f;
        proj[i] = project_3d_to_2d(v, W, H, fov, z_cam);
    }
    SDL_SetRenderDrawColor(R, 255, 255, 255, 255);
    for (int i = 0; i < 12; ++i)
    {
        int a = cube_edges[i][0], b = cube_edges[i][1];
        SDL_RenderDrawLineF(R, proj[a].x, proj[a].y, proj[b].x, proj[b].y);
    }
}
