// Screensaver 4D — C (C11) + SDL2 + OpenMP
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "sim.h"

#ifdef _OPENMP
  #include <omp.h>
#endif

#include <SDL2/SDL.h>

static void set_window_title_fps(SDL_Window* win, float fps) {
    char buf[128];
    snprintf(buf, sizeof(buf), "Screensaver 4D (C)  |  FPS: %.1f", fps);
    SDL_SetWindowTitle(win, buf);
}

static int parse_args(int argc, char** argv, int* N, int* width, int* height,
                      int* threads, unsigned int* seed, int* fpscap)
{
    if (argc < 2) return 0;
    *N = atoi(argv[1]);
    if (*N < 1) *N = 1;
    *width = 1280; *height = 720;
    *threads = -1; *seed = 12345u; *fpscap = 0;

    if (argc >= 4) {
        *width  = atoi(argv[2]); if (*width  < 640) *width  = 640;
        *height = atoi(argv[3]); if (*height < 480) *height = 480;
    }
    for (int i=4; i<argc; ++i) {
        if (strcmp(argv[i], "--threads") == 0 && i+1 < argc) { *threads = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--seed") == 0 && i+1 < argc) { *seed = (unsigned int)strtoul(argv[++i], NULL, 10); }
        else if (strcmp(argv[i], "--fpscap") == 0 && i+1 < argc) { *fpscap = atoi(argv[++i]); }
    }
    return 1;
}

int main(int argc, char** argv)
{
    int N=0, W=1280, H=720, threads=-1, fpscap=0;
    unsigned int seed=12345u;
    if (!parse_args(argc, argv, &N, &W, &H, &threads, &seed, &fpscap)) {
        printf("Uso: %s N [width height] [--seed S] [--fpscap X] [--threads T]\\n", argv[0]);
        return 1;
    }

#ifdef _OPENMP
    if (threads > 0) omp_set_num_threads(threads);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init error: %s\\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Screensaver 4D (C)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- Estado ---
    Particle* particles = (Particle*)malloc(sizeof(Particle) * (size_t)N);
    DrawItem* drawbuf   = (DrawItem*)malloc(sizeof(DrawItem) * (size_t)N);
    if (!particles || !drawbuf) {
        fprintf(stderr, "Error: memoria insuficiente\\n");
        free(particles); free(drawbuf);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }

    init_particles(particles, N, seed);

    const float focal4 = 2.0f;
    const float focal3 = 2.0f;
    int running = 1;

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_prev = SDL_GetPerformanceCounter();
    double acc_time = 0.0;
    double fps_acc = 0.0;
    int fps_count = 0;
    float fps_smoothed = 0.0f;
    Uint32 frame_ticks_start = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        Uint64 t_now = SDL_GetPerformanceCounter();
        double dt = (double)(t_now - t_prev) / (double)perf_freq;
        t_prev = t_now;
        if (dt > 0.1) dt = 0.1;
        float t = (float)(SDL_GetTicks() * 0.001f);

        update_particles((float)dt, particles, drawbuf, N, W, H, t, focal4, focal3);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 8, 13, 255);
        SDL_RenderClear(renderer);

        for (int i=0; i<N; ++i) {
            SDL_SetRenderDrawColor(renderer, drawbuf[i].r8, drawbuf[i].g8, drawbuf[i].b8, drawbuf[i].a8);
            SDL_FRect rect;
            rect.x = drawbuf[i].x - drawbuf[i].r * 0.5f;
            rect.y = drawbuf[i].y - drawbuf[i].r * 0.5f;
            rect.w = drawbuf[i].r;
            rect.h = drawbuf[i].r;
            SDL_RenderFillRectF(renderer, &rect);
        }
        SDL_RenderPresent(renderer);

        acc_time += dt;
        fps_acc += 1.0;
        ++fps_count;
        if (acc_time >= 0.5) {
            float fps = (float)(fps_acc / acc_time);
            fps_smoothed = 0.7f * fps_smoothed + 0.3f * fps;
            set_window_title_fps(window, fps_smoothed);
            acc_time = 0.0; fps_acc = 0.0; fps_count = 0;
        }

        if (fpscap > 0) {
            Uint32 frame_time = SDL_GetTicks() - frame_ticks_start;
            Uint32 target_ms = (Uint32)((1000.0f / (float)fpscap) + 0.5f);
            if (target_ms < 1) target_ms = 1;
            if (frame_time < target_ms) SDL_Delay(target_ms - frame_time);
            frame_ticks_start = SDL_GetTicks();
        }
    }

    free(particles);
    free(drawbuf);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
