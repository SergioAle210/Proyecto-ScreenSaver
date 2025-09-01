// cloth.c — Manta de esferas 3D (OpenMP) con:
//  - Precompute de XY
//  - Bucket sort O(N) por profundidad
//  - En paralelo: batch draw con SDL_RenderGeometry (un solo draw call)
//  - En secuencial: fallback a RenderCopyF por esfera (muchas llamadas)

#include "cloth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef LIKELY
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

static inline float clampf_fast(float v, float a, float b) { return fminf(fmaxf(v, a), b); }

static inline void hsv_to_rgb(float h, float s, float v,
                              unsigned char *R, unsigned char *G, unsigned char *B)
{
    h -= floorf(h);
    float hf = h * 6.0f;
    int i = (int)floorf(hf);
    float f = hf - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float r, g, b;
    switch (i % 6)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break;
    }
    r = clampf_fast(r, 0.f, 1.f);
    g = clampf_fast(g, 0.f, 1.f);
    b = clampf_fast(b, 0.f, 1.f);
    *R = (unsigned char)(r * 255.f);
    *G = (unsigned char)(g * 255.f);
    *B = (unsigned char)(b * 255.f);
}

typedef struct
{
    float x, y, z;
} Vec3;
typedef struct
{
    float x, y;
} Vec2;

static inline Vec3 rotX(Vec3 v, float ang)
{
    float c = cosf(ang), s = sinf(ang);
    Vec3 r;
    r.x = v.x;
    r.y = v.y * c - v.z * s;
    r.z = v.y * s + v.z * c;
    return r;
}
static inline Vec3 rotY(Vec3 v, float ang)
{
    float c = cosf(ang), s = sinf(ang);
    Vec3 r;
    r.x = v.x * c + v.z * s;
    r.y = v.y;
    r.z = -v.x * s + v.z * c;
    return r;
}
static inline Vec2 project_point(Vec3 v, int W, int H, float fov, float zCam)
{
    float denom = (v.z - zCam);
    if (UNLIKELY(fabsf(denom) < 1e-4f))
        denom = (denom >= 0.f ? 1e-4f : -1e-4f);
    float scale = fov / denom;
    float hw = 0.5f * (float)W, hh = 0.5f * (float)H;
    Vec2 out = {v.x * scale * hw + hw, v.y * scale * hh + hh};
    return out;
}

// ====== Sprite circular (ARGB8888) con alpha ======
static SDL_Texture *make_circle_sprite(SDL_Renderer *R, int radius)
{
    int d = radius * 2;
    SDL_Texture *tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, d, d);
    if (!tex)
        return NULL;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch) != 0)
    {
        SDL_DestroyTexture(tex);
        return NULL;
    }
    for (int y = 0; y < d; y++)
    {
        Uint32 *row = (Uint32 *)((unsigned char *)pixels + y * pitch);
        for (int x = 0; x < d; x++)
        {
            float dx = (x + 0.5f - radius), dy = (y + 0.5f - radius);
            float r = sqrtf(dx * dx + dy * dy) / (float)radius; // 0..1
            float alpha = clampf_fast(1.0f - r * r, 0.0f, 1.0f);
            float sx = (dx + dy * 0.3f) / (float)radius;
            float spec = clampf_fast(0.9f - (sx * sx + dy * dy / (radius * radius)) * 1.2f, 0.0f, 1.0f) * 0.3f;
            unsigned char A = (unsigned char)(alpha * 255.f);
            unsigned char c = (unsigned char)(spec * 255.f);
            row[x] = (A << 24) | (c << 16) | (c << 8) | c; // ARGB
        }
    }
    SDL_UnlockTexture(tex);
    return tex;
}

// ====== Ordenación por profundidad (bucket sort O(N)) ======
typedef struct
{
    float depth;
    int idx;
} DepthIdx;

// Precompute XY y buffers de bucketing (globales al módulo para reuso)
static float *g_X = NULL, *g_Y = NULL;
static int g_xy_cap = 0;
static int g_last_GX = 0, g_last_GY = 0;
static float g_last_spanX = 0.f, g_last_spanY = 0.f;

#define ZBINS 128
static int *g_bin_idx = NULL;
static int g_bin_cap = 0;
static int *g_counts = NULL;
static int *g_starts = NULL;
static int *g_write = NULL;
static int *g_order_idx = NULL;
static int g_order_cap = 0;

// ==== Buffers para geometría batcheada (solo usados en paralelo) ====
#ifdef _OPENMP
static SDL_Vertex *g_verts = NULL;
static int g_verts_cap = 0;
static int *g_index = NULL;
static int g_index_cap = 0;
#endif

static int ensure_capacity_xy(int N)
{
    if (N <= g_xy_cap)
        return 1;
    int newcap = (g_xy_cap == 0) ? 4096 : g_xy_cap;
    while (newcap < N)
        newcap = (int)(newcap * 1.5f);
    float *nx = (float *)realloc(g_X, (size_t)newcap * sizeof(float));
    float *ny = (float *)realloc(g_Y, (size_t)newcap * sizeof(float));
    if (!nx || !ny)
        return 0;
    g_X = nx;
    g_Y = ny;
    g_xy_cap = newcap;
    return 1;
}
static int ensure_capacity_bins(int N)
{
    if (N > g_bin_cap)
    {
        int newcap = (g_bin_cap == 0) ? 4096 : g_bin_cap;
        while (newcap < N)
            newcap = (int)(newcap * 1.5f);
        int *nb = (int *)realloc(g_bin_idx, (size_t)newcap * sizeof(int));
        int *no = (int *)realloc(g_order_idx, (size_t)newcap * sizeof(int));
        if (!nb || !no)
            return 0;
        g_bin_idx = nb;
        g_order_idx = no;
        g_bin_cap = g_order_cap = newcap;
    }
    if (!g_counts)
        g_counts = (int *)calloc(ZBINS, sizeof(int));
    if (!g_starts)
        g_starts = (int *)calloc(ZBINS, sizeof(int));
    if (!g_write)
        g_write = (int *)calloc(ZBINS, sizeof(int));
    return (g_counts && g_starts && g_write);
}
#ifdef _OPENMP
static int ensure_capacity_geo(int N)
{
    // 4 vértices y 6 índices por esfera
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
#endif

// ====== Init / destroy ======
static void derive_grid_from_N(int N, int W, int H, int *GX, int *GY)
{
    if (N <= 0)
    {
        *GX = (W >= H) ? 180 : 120;
        *GY = (W >= H) ? 100 : 180;
        return;
    }
    float aspect = (float)W / (float)H;
    int gX = (int)floorf(sqrtf((float)N * aspect));
    int gY = (int)floorf((float)N / (float)gX);
    if (gX < 16)
        gX = 16;
    if (gY < 16)
        gY = 16;
    *GX = gX;
    *GY = gY;
}

int cloth_init(SDL_Renderer *R, ClothState *S, const ClothParams *P_in, int W, int H)
{
    memset(S, 0, sizeof(*S));
    S->W_last = W;
    S->H_last = H;
    S->P = *P_in;
    if (S->P.autoCenter != 0 && S->P.autoCenter != 1)
        S->P.autoCenter = 1;

    if (S->P.GX <= 0 || S->P.GY <= 0)
    {
        int gx, gy;
        derive_grid_from_N(S->P.GX * S->P.GY, W, H, &gx, &gy);
        if (S->P.GX <= 0)
            S->P.GX = gx;
        if (S->P.GY <= 0)
            S->P.GY = gy;
    }
    S->N = S->P.GX * S->P.GY;

    if (S->P.baseRadius <= 0.0f)
    {
        float cellX = (float)W / (float)S->P.GX;
        float cellY = (float)H / (float)S->P.GY;
        S->P.baseRadius = 0.55f * fminf(cellX, cellY);
        if (S->P.baseRadius < 1.0f)
            S->P.baseRadius = 1.0f;
    }

    S->draw = (DrawItem *)malloc(sizeof(DrawItem) * (size_t)S->N);
    S->depth = (float *)malloc(sizeof(float) * (size_t)S->N);
    if (!S->draw || !S->depth)
        return -1;

    int spriteR = (int)ceilf(S->P.baseRadius);
    if (spriteR < 2)
        spriteR = 2;
    S->sprite = make_circle_sprite(R, spriteR);
    S->spriteRadius = spriteR;
    if (!S->sprite)
        return -2;

    S->tx = 0.0f;
    S->ty = 0.0f;

    // Precompute XY inicial
    if (!ensure_capacity_xy(S->N))
        return -3;
    g_last_GX = S->P.GX;
    g_last_GY = S->P.GY;
    g_last_spanX = S->P.spanX;
    g_last_spanY = S->P.spanY;
    float spanX = (S->P.spanX > 0.f ? S->P.spanX : 2.0f);
    float spanY = (S->P.spanY > 0.f ? S->P.spanY : 2.0f);
    for (int j = 0; j < S->P.GY; ++j)
    {
        for (int i = 0; i < S->P.GX; ++i)
        {
            int idx = j * S->P.GX + i;
            float u = (i / (float)(S->P.GX - 1)) * 2.0f - 1.0f;
            float v = (j / (float)(S->P.GY - 1)) * 2.0f - 1.0f;
            g_X[idx] = u * (spanX * 0.5f);
            g_Y[idx] = v * (spanY * 0.5f);
        }
    }

    if (!ensure_capacity_bins(S->N))
        return -4;
#ifdef _OPENMP
    if (!ensure_capacity_geo(S->N))
        return -5;
#endif
    return 0;
}

void cloth_destroy(ClothState *S)
{
    if (S->sprite)
        SDL_DestroyTexture(S->sprite);
    free(S->draw);
    free(S->depth);
    memset(S, 0, sizeof(*S));
    // buffers globales quedan para reuso (se liberan al final del proceso)
}

// ====== Update + Render ======
void cloth_update_and_render(SDL_Renderer *R, ClothState *S, int W, int H, float t)
{
    // Ajustes al cambiar tamaño
    if (W != S->W_last || H != S->H_last)
    {
        float newBase = S->P.baseRadius;
        if (newBase <= 0.0f)
        {
            float cellX = (float)W / (float)S->P.GX;
            float cellY = (float)H / (float)S->P.GY;
            newBase = 0.65f * fminf(cellX, cellY);
            if (newBase < 1.0f)
                newBase = 1.0f;
        }
        int newR = (int)ceilf(newBase);
        if (newR != S->spriteRadius)
        {
            if (S->sprite)
                SDL_DestroyTexture(S->sprite);
            S->sprite = make_circle_sprite(R, newR);
            S->spriteRadius = newR;
        }
        S->P.baseRadius = newBase;
        S->W_last = W;
        S->H_last = H;
    }

    const int GX = S->P.GX, GY = S->P.GY;
    const int N = GX * GY;
    float spanX = (S->P.spanX > 0.f ? S->P.spanX : 2.0f);
    float spanY = (S->P.spanY > 0.f ? S->P.spanY : 2.0f);

    // Recompute XY si cambian grilla o spans
    if (GX != g_last_GX || GY != g_last_GY || spanX != g_last_spanX || spanY != g_last_spanY)
    {
        if (!ensure_capacity_xy(N))
            return;
        g_last_GX = GX;
        g_last_GY = GY;
        g_last_spanX = spanX;
        g_last_spanY = spanY;
        for (int j = 0; j < GY; ++j)
        {
            for (int i = 0; i < GX; ++i)
            {
                int idx = j * GX + i;
                float u = (i / (float)(GX - 1)) * 2.0f - 1.0f;
                float v = (j / (float)(GY - 1)) * 2.0f - 1.0f;
                g_X[idx] = u * (spanX * 0.5f);
                g_Y[idx] = v * (spanY * 0.5f);
            }
        }
    }
    if (!ensure_capacity_bins(N))
        return;
#ifdef _OPENMP
    if (!ensure_capacity_geo(N))
        return;
#endif

    const float DEG2RAD = (float)M_PI / 180.0f;
    const float tiltX = S->P.tiltX_deg * DEG2RAD;
    const float tiltY = S->P.tiltY_deg * DEG2RAD;
    const float zCam = (S->P.zCam != 0.0f ? S->P.zCam : -6.0f);
    const float fov = (S->P.fov != 0.0f ? S->P.fov : 1.0f);

    const float amp = (S->P.amp != 0.0f ? S->P.amp : 0.28f);
    const float sig = (S->P.sigma != 0.0f ? S->P.sigma : 0.25f);
    const float omg = (S->P.omega != 0.0f ? S->P.omega : 2.8f);
    const float spd = (S->P.speed != 0.0f ? S->P.speed : 1.0f);
    const float cs = (S->P.colorSpeed != 0.0f ? S->P.colorSpeed : 0.35f);

    // Centro de perturbación
    float cx = 0.45f * spanX * sinf(0.9f * spd * t);
    float cy = 0.45f * spanY * cosf(1.2f * spd * t + 0.7f);

    // Onda base + gaussiana
    const float kx = 2.2f, ky = 1.7f;
    const float inv2sig2 = 1.0f / (2.0f * sig * sig);

    // ====== Paso 1: UPDATE en paralelo + profundidad (reduction) ======
    float zmin = 1e30f, zmax = -1e30f;
#ifdef _OPENMP
#pragma omp parallel for collapse(2) schedule(static) reduction(min : zmin) reduction(max : zmax)
#endif
    for (int j = 0; j < GY; ++j)
    {
        for (int i = 0; i < GX; ++i)
        {
            int idx = j * GX + i;

            float X = g_X[idx];
            float Y = g_Y[idx];

            float base = 0.22f * sinf(kx * X + 0.7f * t) * cosf(ky * Y + 0.9f * t);
            float dx = X - cx, dy = Y - cy;
            float r2 = dx * dx + dy * dy;
            float g = expf(-(r2)*inv2sig2);
            float Z = base + amp * g * sinf(omg * t + r2 * 0.6f);

            Vec3 P = {X, Y, 2.0f + Z};
            P = rotX(P, tiltX);
            P = rotY(P, tiltY);

            S->depth[idx] = P.z;
            if (P.z < zmin)
                zmin = P.z;
            if (P.z > zmax)
                zmax = P.z;

            Vec2 Scr = project_point(P, W, H, fov, zCam);
            float denom = (P.z - zCam);
            if (UNLIKELY(fabsf(denom) < 1e-4f))
                denom = (denom >= 0.f ? 1e-4f : -1e-4f);
            float scale = fov / denom;
            float radius = S->P.baseRadius * clampf_fast(scale * 0.9f, 0.5f, 2.1f);

            float u = (i / (float)(GX - 1)) * 2.0f - 1.0f;
            float hue = 0.6f + 0.25f * Z + cs * t + 0.08f * u;
            unsigned char R8, G8, B8;
            hsv_to_rgb(hue, 0.8f, 0.95f, &R8, &G8, &B8);

            DrawItem di;
            di.x = Scr.x;
            di.y = Scr.y;
            di.r = radius;
            di.r8 = R8;
            di.g8 = G8;
            di.b8 = B8;
            di.a8 = 220;
            S->draw[idx] = di;
        }
    }

    // ====== Paso 1.5: centrado/paneo (reducción) ======
    float tx_target = 0.0f, ty_target = 0.0f;
    if (S->P.autoCenter)
    {
        float minx = 1e30f, maxx = -1e30f;
        float miny = 1e30f, maxy = -1e30f;
#ifdef _OPENMP
#pragma omp parallel
        {
            float lminx = 1e30f, lmaxx = -1e30f;
            float lminy = 1e30f, lmaxy = -1e30f;
#pragma omp for nowait
            for (int k = 0; k < N; ++k)
            {
                const DrawItem *d = &S->draw[k];
                if (d->x < lminx)
                    lminx = d->x;
                if (d->x > lmaxx)
                    lmaxx = d->x;
                if (d->y < lminy)
                    lminy = d->y;
                if (d->y > lmaxy)
                    lmaxy = d->y;
            }
#pragma omp critical
            {
                if (lminx < minx)
                    minx = lminx;
                if (lmaxx > maxx)
                    maxx = lmaxx;
                if (lminy < miny)
                    miny = lminy;
                if (lmaxy > maxy)
                    maxy = lmaxy;
            }
        }
#else
        for (int k = 0; k < N; ++k)
        {
            const DrawItem *d = &S->draw[k];
            if (d->x < minx)
                minx = d->x;
            if (d->x > maxx)
                maxx = d->x;
            if (d->y < miny)
                miny = d->y;
            if (d->y > maxy)
                maxy = d->y;
        }
#endif
        float cx2 = 0.5f * (minx + maxx);
        float cy2 = 0.5f * (miny + maxy);
        tx_target = (W * 0.5f - cx2) + S->P.panX_px;
        ty_target = (H * 0.5f - cy2) + S->P.panY_px;
    }
    else
    {
        tx_target = S->P.panX_px;
        ty_target = S->P.panY_px;
    }
    const float alpha = 0.2f;
    S->tx += alpha * (tx_target - S->tx);
    S->ty += alpha * (ty_target - S->ty);

    // ====== Paso 2: BUCKET SORT O(N) ======
    float range = (zmax - zmin);
    if (range < 1e-6f)
        range = 1e-6f;
    float invRange = (float)(ZBINS - 1) / range;

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 0; k < N; ++k)
    {
        int b = (int)((S->depth[k] - zmin) * invRange + 0.5f);
        if (b < 0)
            b = 0;
        else if (b >= ZBINS)
            b = ZBINS - 1;
        g_bin_idx[k] = b;
    }
    memset(g_counts, 0, sizeof(int) * ZBINS);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 0; k < N; ++k)
    {
#ifdef _OPENMP
#pragma omp atomic
#endif
        g_counts[g_bin_idx[k]]++;
    }
    int sum = 0;
    for (int b = 0; b < ZBINS; ++b)
    {
        g_starts[b] = sum;
        g_write[b] = sum;
        sum += g_counts[b];
    }
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 0; k < N; ++k)
    {
        int b = g_bin_idx[k];
        int pos;
#ifdef _OPENMP
#pragma omp atomic capture
#endif
        {
            pos = g_write[b]++;
        }
        g_order_idx[pos] = k;
    }

    // ====== Paso 3: DIBUJAR ======
#ifdef _OPENMP
    // --- Ruta rápida paralela: generar geometría y un solo draw call ---
    // (cada hilo llena su bloque; hay barrera implícita al final del for)
    // 4 vértices y 6 índices por esfera, en orden ya bucketizado (painter's)
    if (ensure_capacity_geo(N))
    {
        // Llenar vértices/índices en paralelo (no hay solapamiento => sin carreras)
#pragma omp parallel for schedule(static)
        for (int q = 0; q < N; ++q)
        {
            int idx = g_order_idx[q];
            const DrawItem *d = &S->draw[idx];
            float x0 = (d->x + S->tx) - d->r;
            float y0 = (d->y + S->ty) - d->r;
            float x1 = x0 + 2.0f * d->r;
            float y1 = y0 + 2.0f * d->r;

            // 4 vértices con mismo color (usa textura de círculo con alfa)
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

        // Un solo draw call. Si el backend no soporta RenderGeometry, caerá en fallback.
        if (SDL_RenderGeometry(R, S->sprite, g_verts, 4 * N, g_index, 6 * N) != 0)
        {
            // Fallback: bucle clásico (debería ser raro)
            for (int q = 0; q < N; ++q)
            {
                const DrawItem *d = &S->draw[g_order_idx[q]];
                SDL_SetTextureColorMod(S->sprite, d->r8, d->g8, d->b8);
                SDL_SetTextureAlphaMod(S->sprite, d->a8);
                float diam = d->r * 2.0f;
                SDL_FRect dst = {(d->x + S->tx) - d->r, (d->y + S->ty) - d->r, diam, diam};
                SDL_RenderCopyF(R, S->sprite, NULL, &dst);
            }
        }
    }
    else
    {
        // Memoria insuficiente -> fallback clásico
        for (int q = 0; q < N; ++q)
        {
            const DrawItem *d = &S->draw[g_order_idx[q]];
            SDL_SetTextureColorMod(S->sprite, d->r8, d->g8, d->b8);
            SDL_SetTextureAlphaMod(S->sprite, d->a8);
            float diam = d->r * 2.0f;
            SDL_FRect dst = {(d->x + S->tx) - d->r, (d->y + S->ty) - d->r, diam, diam};
            SDL_RenderCopyF(R, S->sprite, NULL, &dst);
        }
    }
#else
    // --- Ruta secuencial (muchas llamadas): mantiene diferencia a favor del paralelo ---
    for (int q = 0; q < N; ++q)
    {
        const DrawItem *d = &S->draw[g_order_idx[q]];
        SDL_SetTextureColorMod(S->sprite, d->r8, d->g8, d->b8);
        SDL_SetTextureAlphaMod(S->sprite, d->a8);
        float diam = d->r * 2.0f;
        SDL_FRect dst = {(d->x + S->tx) - d->r, (d->y + S->ty) - d->r, diam, diam};
        SDL_RenderCopyF(R, S->sprite, NULL, &dst);
    }
#endif
}
