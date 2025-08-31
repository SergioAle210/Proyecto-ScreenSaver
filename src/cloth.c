// cloth.c — Manta de esferas 3D paralelizada con OpenMP, “aguja” animada y color HSV
#include "cloth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#if defined(__GNUC__) || defined(__clang__)
#include <alloca.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif

// ================== Utilitarios ==================
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static inline void hsv_to_rgb(float h, float s, float v, unsigned char *R, unsigned char *G, unsigned char *B)
{
    h -= floorf(h); // wrap [0,1)
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float r, g, b;
    switch (((int)i) % 6)
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
    *R = (unsigned char)(clampf(r, 0, 1) * 255.0f);
    *G = (unsigned char)(clampf(g, 0, 1) * 255.0f);
    *B = (unsigned char)(clampf(b, 0, 1) * 255.0f);
}

// Proyección / rotaciones
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
    return (Vec3){v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}
static inline Vec3 rotY(Vec3 v, float ang)
{
    float c = cosf(ang), s = sinf(ang);
    return (Vec3){v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}
static inline Vec2 project_point(Vec3 v, int W, int H, float fov, float zCam)
{
    float scale = fov / (v.z - zCam);
    return (Vec2){(v.x * scale * W * 0.5f) + W * 0.5f, (v.y * scale * H * 0.5f) + H * 0.5f};
}

// Sprite circular con gradiente suave (ARGB8888)
static SDL_Texture *make_circle_sprite(SDL_Renderer *R, int radius)
{
    int d = radius * 2;
    SDL_Texture *tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, d, d);
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
            float dx = (x + 0.5f - radius);
            float dy = (y + 0.5f - radius);
            float r = sqrtf(dx * dx + dy * dy) / (float)radius; // 0..1
            float alpha = clampf(1.0f - r * r, 0.0f, 1.0f);     // borde suave
            float sx = (dx + dy * 0.3f) / (float)radius;        // leve “specular”
            float spec = clampf(0.9f - (sx * sx + dy * dy / (radius * radius)) * 1.2f, 0.0f, 1.0f) * 0.3f;
            unsigned char A = (unsigned char)(alpha * 255.0f);
            unsigned char c = (unsigned char)(spec * 255.0f);
            row[x] = (A << 24) | (c << 16) | (c << 8) | c; // ARGB
        }
    }
    SDL_UnlockTexture(tex);
    return tex;
}

// Ordenación por profundidad (simple qsort)
typedef struct
{
    float depth;
    int idx;
} DepthIdx;
static int cmp_depth(const void *a, const void *b)
{
    float da = ((const DepthIdx *)a)->depth, db = ((const DepthIdx *)b)->depth;
    return (da < db) ? -1 : (da > db) ? 1
                                      : 0; // back-to-front
}

// ================== Inicialización / parámetros ==================
static void derive_grid_from_N(int N, int W, int H, int *GX, int *GY)
{
    if (N <= 0)
    { // por defecto
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

    // Defaults de centrado/pan
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

    // Radio base: solo lo derivamos si el usuario NO pasó --radius
    if (S->P.baseRadius <= 0.0f)
    {
        float cellX = (float)W / (float)S->P.GX;
        float cellY = (float)H / (float)S->P.GY;
        S->P.baseRadius = 0.55f * fminf(cellX, cellY);
        if (S->P.baseRadius < 1.0f)
            S->P.baseRadius = 1.0f;
    }

    S->draw = (DrawItem *)malloc(sizeof(DrawItem) * S->N);
    S->depth = (float *)malloc(sizeof(float) * S->N);
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
    return 0;
}

void cloth_destroy(ClothState *S)
{
    if (S->sprite)
        SDL_DestroyTexture(S->sprite);
    free(S->draw);
    free(S->depth);
    memset(S, 0, sizeof(*S));
}

// ================== Núcleo: actualizar + render ==================
void cloth_update_and_render(SDL_Renderer *R, ClothState *S, int W, int H, float t)
{
    // Ajuste de sprite / radio si cambia tamaño ventana:
    if (W != S->W_last || H != S->H_last)
    {
        float newBase = S->P.baseRadius;
        if (newBase <= 0.0f)
        { // solo recalcular si NO hubo override del usuario
            float cellX = (float)W / (float)S->P.GX;
            float cellY = (float)H / (float)S->P.GY;
            newBase = 0.65f * fminf(cellX, cellY); // un poco mayor que 0.55
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
    const float spanX = (S->P.spanX > 0.0f ? S->P.spanX : 2.0f);
    const float spanY = (S->P.spanY > 0.0f ? S->P.spanY : 2.0f);

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

    // Centro de perturbación tipo Lissajous
    float cx = 0.45f * spanX * sinf(0.9f * spd * t);
    float cy = 0.45f * spanY * cosf(1.2f * spd * t + 0.7f);

    // Onda base + gaussiana de la aguja
    const float kx = 2.2f, ky = 1.7f;
    const float inv2sig2 = 1.0f / (2.0f * sig * sig);

// ====== Paso 1: actualizar draw items en paralelo ======
#pragma omp parallel for if (GX * GY > 1024) schedule(static)
    for (int j = 0; j < GY; ++j)
    {
        for (int i = 0; i < GX; ++i)
        {
            int idx = j * GX + i;

            // Coordenadas “mundo”
            float u = (i / (float)(GX - 1)) * 2.0f - 1.0f; // -1..1
            float v = (j / (float)(GY - 1)) * 2.0f - 1.0f; // -1..1
            float X = u * spanX * 0.5f;
            float Y = v * spanY * 0.5f;

            float base = 0.22f * sinf(kx * X + 0.7f * t) * cosf(ky * Y + 0.9f * t);
            float dx = X - cx, dy = Y - cy;
            float g = expf(-(dx * dx + dy * dy) * inv2sig2);
            float Z = base + amp * g * sinf(omg * t + (dx * dx + dy * dy) * 0.6f);

            Vec3 P = {X, Y, 2.0f + Z}; // delante de la cámara
            P = rotX(P, tiltX);
            P = rotY(P, tiltY);

            Vec2 Scr = project_point(P, W, H, fov, zCam);

            float scale = fov / (P.z - zCam);
            float radius = S->P.baseRadius * clampf(scale * 0.9f, 0.5f, 2.1f);

            float hue = 0.6f + 0.25f * Z + cs * t + 0.08f * u; // [0,1)
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
            S->depth[idx] = P.z;
        }
    }

    // ====== Paso 1.5: centrado/paneo en pantalla ======
    float tx_target = 0.0f, ty_target = 0.0f;
    if (S->P.autoCenter)
    {
        float minx = 1e30f, maxx = -1e30f;
        float miny = 1e30f, maxy = -1e30f;
        for (int k = 0; k < S->N; ++k)
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
    // Suavizado para evitar jitter
    const float alpha = 0.2f;
    S->tx += alpha * (tx_target - S->tx);
    S->ty += alpha * (ty_target - S->ty);

    // ====== Paso 2: ordenar por profundidad ======
    DepthIdx *order = (DepthIdx *)alloca(sizeof(DepthIdx) * S->N);
    for (int k = 0; k < S->N; ++k)
    {
        order[k].depth = S->depth[k];
        order[k].idx = k;
    }
    qsort(order, S->N, sizeof(DepthIdx), cmp_depth);

    // ====== Paso 3: dibujar ======
    for (int q = 0; q < S->N; ++q)
    {
        const DrawItem *d = &S->draw[order[q].idx];
        SDL_SetTextureColorMod(S->sprite, d->r8, d->g8, d->b8);
        SDL_SetTextureAlphaMod(S->sprite, d->a8);
        float diam = d->r * 2.0f;
        SDL_FRect dst = {(d->x + S->tx) - d->r, (d->y + S->ty) - d->r, diam, diam};
        SDL_RenderCopyF(R, S->sprite, NULL, &dst);
    }
}
