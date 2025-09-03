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

// ---------- utilidades ----------
static inline float clampf(float v, float a, float b) { return fminf(fmaxf(v, a), b); }

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
    r = clampf(r, 0.f, 1.f);
    g = clampf(g, 0.f, 1.f);
    b = clampf(b, 0.f, 1.f);
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

// ---------- sprite circular (ARGB8888) ----------
static SDL_Texture *make_circle_sprite(SDL_Renderer *R, int radius)
{
    int d = radius * 2;
    SDL_Texture *tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STATIC, d, d);
    if (!tex)
        return NULL;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    // buffer CPU temporal
    const int pitch = d * (int)sizeof(Uint32);
    Uint32 *buf = (Uint32 *)malloc((size_t)pitch * d);
    if (!buf)
    {
        SDL_DestroyTexture(tex);
        return NULL;
    }

    for (int y = 0; y < d; ++y)
    {
        Uint32 *row = buf + y * d;
        for (int x = 0; x < d; ++x)
        {
            float dx = (x + 0.5f - radius), dy = (y + 0.5f - radius);
            float r = sqrtf(dx * dx + dy * dy) / (float)radius; // 0..1
            float alpha = clampf(1.0f - r * r, 0.0f, 1.0f);     // borde suave
            float sx = (dx + dy * 0.3f) / (float)radius;
            float spec = clampf(0.9f - (sx * sx + dy * dy / (radius * radius)) * 1.2f, 0.0f, 1.0f) * 0.3f;
            Uint8 A = (Uint8)(alpha * 255.f);
            Uint8 c = (Uint8)(spec * 255.f);
            row[x] = (A << 24) | (c << 16) | (c << 8) | c; // ARGB
        }
    }
    SDL_UpdateTexture(tex, NULL, buf, pitch);
    free(buf);
    return tex;
}

// ---------- buffers globales para reuso ----------
static float *g_X = NULL, *g_Y = NULL; // precompute XY
static int g_xy_cap = 0;
static int g_last_GX = 0, g_last_GY = 0;
static float g_last_spanX = 0.f, g_last_spanY = 0.f;

#define ZBINS 128
static int *g_bin_idx = NULL; // N
static int g_bin_cap = 0;
static int *g_counts = NULL; // ZBINS
static int *g_starts = NULL; // ZBINS
static int *g_write = NULL;  // ZBINS

// ---------- ensure capacities ----------
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
        if (!nb)
            return 0;
        g_bin_idx = nb;
        g_bin_cap = newcap;
    }
    if (!g_counts)
        g_counts = (int *)calloc(ZBINS, sizeof(int));
    if (!g_starts)
        g_starts = (int *)calloc(ZBINS, sizeof(int));
    if (!g_write)
        g_write = (int *)calloc(ZBINS, sizeof(int));
    return (g_counts && g_starts && g_write);
}
static int ensure_capacity_order(ClothState *S, int N)
{
    if (N <= S->order_cap)
        return 1;
    int newcap = (S->order_cap == 0) ? 4096 : S->order_cap;
    while (newcap < N)
        newcap = (int)(newcap * 1.5f);
    int *no = (int *)realloc(S->order_idx, (size_t)newcap * sizeof(int));
    if (!no)
        return 0;
    S->order_idx = no;
    S->order_cap = newcap;
    return 1;
}

// ---------- helpers ----------
static void derive_grid_from_N(int N, int W, int H, int *GX, int *GY)
{
    if (N <= 0)
    { // razonable por defecto
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

// ---------- API ----------
int cloth_init(SDL_Renderer *R, ClothState *S, const ClothParams *P_in, int W, int H)
{
    if (!R || !S || !P_in || W <= 0 || H <= 0)
        return -1;

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
    if (S->N <= 0)
        return -2;

    // Radio base automático si hace falta
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
        return -3;

    int spriteR = (int)ceilf(S->P.baseRadius);
    if (spriteR < 2)
        spriteR = 2;
    S->sprite = make_circle_sprite(R, spriteR);
    S->spriteRadius = spriteR;
    if (!S->sprite)
        return -4;

    // Precompute XY inicial
    if (!ensure_capacity_xy(S->N))
        return -5;
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
        return -6;
    if (!ensure_capacity_order(S, S->N))
        return -7;

    S->tx = 0.f;
    S->ty = 0.f;
    return 0;
}

void cloth_destroy(ClothState *S)
{
    if (!S)
        return;
    if (S->sprite)
        SDL_DestroyTexture(S->sprite);
    free(S->draw);
    S->draw = NULL;
    free(S->depth);
    S->depth = NULL;
    free(S->order_idx);
    S->order_idx = NULL;
    S->order_cap = 0;
}

void cloth_update(SDL_Renderer *R, ClothState *S, int W, int H, float t)
{
    (void)R; // no se usa aquí
    if (!S || W <= 0 || H <= 0)
        return;

    // Ajustes al cambiar tamaño (recrea sprite si el radio base cambia)
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
    if (!ensure_capacity_order(S, N))
        return;

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

    // Centro de perturbación (suave)
    float cx = 0.45f * spanX * sinf(0.9f * spd * t);
    float cy = 0.45f * spanY * cosf(1.2f * spd * t + 0.7f);

    // Onda base + gaussiana
    const float kx = 2.2f, ky = 1.7f;
    const float inv2sig2 = 1.0f / (2.0f * sig * sig);

    // ---- Paso 1: UPDATE + profundidad (reductions) ----
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
            float radius = S->P.baseRadius * clampf(scale * 0.9f, 0.5f, 2.1f);

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

    // ---- Paso 1.5: centrado/paneo (reducción en bbox) ----
    float tx_target = S->P.panX_px, ty_target = S->P.panY_px;
    if (S->P.autoCenter)
    {
        float minx = 1e30f, maxx = -1e30f, miny = 1e30f, maxy = -1e30f;
#ifdef _OPENMP
#pragma omp parallel
        {
            float lminx = 1e30f, lmaxx = -1e30f, lminy = 1e30f, lmaxy = -1e30f;
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
    const float alpha = 0.2f;
    S->tx += alpha * (tx_target - S->tx);
    S->ty += alpha * (ty_target - S->ty);

    // ---- Paso 2: BUCKET SORT O(N) ----
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
        pos = g_write[b]++;
        S->order_idx[pos] = k;
    }
}
