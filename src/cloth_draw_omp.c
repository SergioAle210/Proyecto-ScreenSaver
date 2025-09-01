// cloth_draw_omp.c — backend paralelo con OpenMP y SDL_RenderGeometry
#include "cloth.h"
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

// Buffers de geometría locales al backend
static SDL_Vertex *g_verts = NULL;
static int g_verts_cap = 0;
static int *g_index = NULL;
static int g_index_cap = 0;

static int ensure_capacity_geo(int N)
{
    // 4 vértices y 6 índices por sprite
    int needV = 4 * N, needI = 6 * N;
    if (needV > g_verts_cap)
    {
        int newV = (g_verts_cap == 0) ? (4 * 4096) : g_verts_cap;
        while (newV < needV)
            newV = (int)(newV * 1.5f);
        SDL_Vertex *nv = (SDL_Vertex *)realloc(g_verts, (size_t)newV * sizeof(SDL_Vertex));
        if (!nv)
            return 0;
        g_verts = nv;
        g_verts_cap = newV;
    }
    if (needI > g_index_cap)
    {
        int newI = (g_index_cap == 0) ? (6 * 4096) : g_index_cap;
        while (newI < needI)
            newI = (int)(newI * 1.5f);
        int *ni = (int *)realloc(g_index, (size_t)newI * sizeof(int));
        if (!ni)
            return 0;
        g_index = ni;
        g_index_cap = newI;
    }
    return 1;
}

void cloth_draw_omp_release(void)
{
    free(g_verts);
    g_verts = NULL;
    g_verts_cap = 0;
    free(g_index);
    g_index = NULL;
    g_index_cap = 0;
}

void cloth_render_omp(SDL_Renderer *R, const ClothState *S)
{
    const int N = S->N;

#if defined(_OPENMP)
    if (N <= 0 || S->sprite == NULL || !ensure_capacity_geo(N))
    {
        cloth_render_seq(R, S);
        return;
    }

#pragma omp parallel for schedule(static)
    for (int q = 0; q < N; ++q)
    {
        int idx = S->order_idx[q];
        const DrawItem *d = &S->draw[idx];

        float x0 = (d->x + S->tx) - d->r;
        float y0 = (d->y + S->ty) - d->r;
        float x1 = x0 + 2.0f * d->r;
        float y1 = y0 + 2.0f * d->r;

        SDL_Color col = {d->r8, d->g8, d->b8, d->a8};
        int v = 4 * q, ii = 6 * q;

        g_verts[v + 0].position.x = x0;
        g_verts[v + 0].position.y = y0;
        g_verts[v + 0].color = col;
        g_verts[v + 0].tex_coord.x = 0.f;
        g_verts[v + 0].tex_coord.y = 0.f;

        g_verts[v + 1].position.x = x1;
        g_verts[v + 1].position.y = y0;
        g_verts[v + 1].color = col;
        g_verts[v + 1].tex_coord.x = 1.f;
        g_verts[v + 1].tex_coord.y = 0.f;

        g_verts[v + 2].position.x = x1;
        g_verts[v + 2].position.y = y1;
        g_verts[v + 2].color = col;
        g_verts[v + 2].tex_coord.x = 1.f;
        g_verts[v + 2].tex_coord.y = 1.f;

        g_verts[v + 3].position.x = x0;
        g_verts[v + 3].position.y = y1;
        g_verts[v + 3].color = col;
        g_verts[v + 3].tex_coord.x = 0.f;
        g_verts[v + 3].tex_coord.y = 1.f;

        g_index[ii + 0] = v + 0;
        g_index[ii + 1] = v + 1;
        g_index[ii + 2] = v + 2;
        g_index[ii + 3] = v + 2;
        g_index[ii + 4] = v + 3;
        g_index[ii + 5] = v + 0;
    }

    // Si falla el draw (renderer sin soporte) -> secuencial
    if (SDL_RenderGeometry(R, S->sprite, g_verts, 4 * N, g_index, 6 * N) != 0)
    {
        cloth_render_seq(R, S);
    }
#else
    cloth_render_seq(R, S);
#endif
}
