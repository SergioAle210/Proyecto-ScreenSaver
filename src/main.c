// main.c — modo: cloth — SDL2 + (opcional) OpenMP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "sim.h"
#include "cloth.h"

enum Mode
{
    MODE_CLOTH = 0
};

static void print_usage(const char *prog)
{
    printf("Uso: %s N [opciones]\n", prog);
    printf("  --mode cloth\n");
    printf("  --fpscap X       (limite de FPS; 0 = sin limite)\n");
#ifdef _OPENMP
    printf("  --threads T      (OpenMP threads)\n");
    printf("  --nogeom         (diagnostico: fuerza backend secuencial)\n");
#endif
    printf("  --novsync        (desactiva vsync del renderer)\n");
    printf("\nModo cloth (manta):\n");
    printf("  --grid GXxGY     (p. ej. 180x100; si se omite, se deriva de N/aspecto)\n");
    printf("  --tilt DEG       (inclinacion X en grados)\n");
    printf("  --fov  F         (campo de vision; ~1.0..2.2)\n");
    printf("  --zcam Z         (posicion camara; mas cerca: -3.0)\n");
    printf("  --spanX Sx       (ancho “mundo”)\n");
    printf("  --spanY Sy       (alto  “mundo”)\n");
    printf("  --radius R       (radio base por bola en px; override)\n");
    printf("  --amp  A         (amplitud aguja)\n");
    printf("  --sigma S        (dispersion aguja)\n");
    printf("  --speed V        (velocidad aguja)\n");
    printf("  --colorSpeed C   (velocidad ciclo color)\n");
    printf("  --panX px        (paneo horizontal en pixeles; + derecha)\n");
    printf("  --panY px        (paneo vertical en pixeles; + abajo)\n");
    printf("  --center 0|1     (centrado automatico; default 1)\n");
}

static int parse_grid(const char *s, int *GX, int *GY)
{
    int a = 0, b = 0;
    if (sscanf(s, "%dx%d", &a, &b) == 2 && a > 0 && b > 0)
    {
        *GX = a;
        *GY = b;
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    enum Mode mode = MODE_CLOTH;
    int fpscap = 0; // 0 = sin limite
    bool vsync_on = true;
#ifdef _OPENMP
    bool noGeom = false; // fuerza backend secuencial desde binario paralelo
    int threads = 0;     // 0 -> decide runtime
#endif

    // ---- Parametros CLOTH ----
    ClothParams CP = {0};
    CP.GX = 0; // si 0, se deriva de N/aspecto
    CP.GY = 0;
    CP.spanX = 2.4f;
    CP.spanY = 1.8f;
    CP.tiltX_deg = 22.0f;
    CP.tiltY_deg = -8.0f;
    CP.zCam = -6.0f;
    CP.fov = 1.05f;
    CP.baseRadius = 0.0f; // si no hay --radius, se deriva
    CP.amp = 0.28f;
    CP.sigma = 0.25f;
    CP.omega = 2.8f;
    CP.speed = 1.0f;
    CP.colorSpeed = 0.35f;
    CP.panX_px = 0.0f;
    CP.panY_px = 0.0f;
    CP.autoCenter = 1;

    // ---- Parseo CLI ----
    for (int i = 2; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--mode") && i + 1 < argc)
        {
            const char *m = argv[++i];
            if (!strcmp(m, "cloth"))
                mode = MODE_CLOTH;
        }
        else if (!strcmp(argv[i], "--fpscap") && i + 1 < argc)
        {
            fpscap = atoi(argv[++i]);
#ifdef _OPENMP
        }
        else if (!strcmp(argv[i], "--threads") && i + 1 < argc)
        {
            threads = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--nogeom"))
        {
            noGeom = true;
#endif
        }
        else if (!strcmp(argv[i], "--novsync"))
        {
            vsync_on = false;
        }
        else if (!strcmp(argv[i], "--grid") && i + 1 < argc)
        {
            if (!parse_grid(argv[++i], &CP.GX, &CP.GY))
            {
                fprintf(stderr, "Formato --grid invalido. Use GXxGY, p.ej. 180x100\n");
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--tilt") && i + 1 < argc)
        {
            CP.tiltX_deg = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--fov") && i + 1 < argc)
        {
            CP.fov = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--zcam") && i + 1 < argc)
        {
            CP.zCam = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--spanX") && i + 1 < argc)
        {
            CP.spanX = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--spanY") && i + 1 < argc)
        {
            CP.spanY = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--radius") && i + 1 < argc)
        {
            CP.baseRadius = (float)atof(argv[++i]); // px
        }
        else if (!strcmp(argv[i], "--amp") && i + 1 < argc)
        {
            CP.amp = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--sigma") && i + 1 < argc)
        {
            CP.sigma = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--speed") && i + 1 < argc)
        {
            CP.speed = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--colorSpeed") && i + 1 < argc)
        {
            CP.colorSpeed = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--panX") && i + 1 < argc)
        {
            CP.panX_px = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--panY") && i + 1 < argc)
        {
            CP.panY_px = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--center") && i + 1 < argc)
        {
            CP.autoCenter = atoi(argv[++i]) ? 1 : 0;
        }
        else
        {
            fprintf(stderr, "Argumento no reconocido: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

#ifdef _OPENMP
    if (threads > 0)
    {
        omp_set_num_threads(threads);
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 3;
    }

    // Hints de rendimiento (no rompen nada)
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_DisplayMode DM;
    if (SDL_GetCurrentDisplayMode(0, &DM) != 0)
    {
        DM.w = 1280;
        DM.h = 720; // fallback
    }
    int W = DM.w, H = DM.h;

    SDL_Window *win = SDL_CreateWindow(
        "Screensaver",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H,
        SDL_WINDOW_RESIZABLE);
    if (!win)
    {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 4;
    }
    SDL_MaximizeWindow(win);

    Uint32 rflags = SDL_RENDERER_ACCELERATED | (vsync_on ? SDL_RENDERER_PRESENTVSYNC : 0);
    SDL_Renderer *R = SDL_CreateRenderer(win, -1, rflags);
    if (!R)
    {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        return 4;
    }

    // --- Init modo cloth ---
    ClothState CS;
    memset(&CS, 0, sizeof(CS));
    if (mode == MODE_CLOTH)
    {
        // si usuario paso N sin grid, deriva una malla 1xN (init la corrige)
        if ((CP.GX <= 0 || CP.GY <= 0) && N > 0)
        {
            CP.GX = N;
            CP.GY = 1;
        }
        if (cloth_init(R, &CS, &CP, W, H) != 0)
        {
            fprintf(stderr, "Error inicializando CLOTH\n");
            SDL_DestroyRenderer(R);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 5;
        }
    }

    int running = 1;
    Uint32 last = SDL_GetTicks();
    float t = 0.0f;
    int frame_count = 0;
    Uint32 fps_timer = SDL_GetTicks();
    int fps = 0;

#ifdef _OPENMP
    int omp_on = 1;
    int omp_threads = (threads > 0) ? threads : omp_get_max_threads();
#else
    int omp_on = 0;
    int omp_threads = 1;
#endif

    // ---------------- Bucle principal ----------------
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last) * (1.0f / 1000.0f);
        if (dt <= 0.0f)
            dt = 1.0f / 1000.0f;
        last = now;
        t += dt;

        SDL_GetWindowSize(win, &W, &H);
        SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
        SDL_RenderClear(R);

        if (mode == MODE_CLOTH)
        {
            cloth_update(R, &CS, W, H, t);
#ifdef _OPENMP
            if (omp_on && !noGeom)
                cloth_render_omp(R, &CS); // batch geometry + draw call único
            else
                cloth_render_seq(R, &CS); // fallback secuencial
#else
            cloth_render_seq(R, &CS);
#endif
        }

        SDL_RenderPresent(R);
        frame_count++;

        if (SDL_GetTicks() - fps_timer >= 1000)
        {
            fps = frame_count;
            frame_count = 0;
            fps_timer = SDL_GetTicks();
            char title[256];
            SDL_RendererInfo info;
            SDL_GetRendererInfo(R, &info);
            snprintf(title, sizeof(title),
                     "Screensaver | Mode=cloth | %dx%d | FPS:%d | OMP:%s T=%d | Rndr:%s",
                     W, H, fps, (omp_on ? "ON" : "OFF"), omp_threads,
                     info.name ? info.name : "unknown");
            SDL_SetWindowTitle(win, title);
        }

        if (fpscap > 0)
        {
            Uint32 frame_time = SDL_GetTicks() - now;
            Uint32 delay = 1000u / (Uint32)fpscap;
            if (frame_time < delay)
                SDL_Delay(delay - frame_time);
        }
    }

    if (mode == MODE_CLOTH)
        cloth_destroy(&CS);
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
