// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "sim.h"
#include "cube3d.h"

enum Mode { MODE_PARTICLES=0, MODE_CUBE3D=1 };

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s N [opciones]\n", argv[0]);
        printf("Opciones: --mode particles|cube3d\n");
        return 1;
    }

    int N = atoi(argv[1]), maxfps = 60;
    enum Mode mode = MODE_PARTICLES;

    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
            ++i;
            if (!strcmp(argv[i], "particles")) mode = MODE_PARTICLES;
            else if (!strcmp(argv[i], "cube3d")) mode = MODE_CUBE3D;
        }
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_DisplayMode DM;
    SDL_GetCurrentDisplayMode(0, &DM);
    int W = DM.w;
    int H = DM.h;

    SDL_Window* win = SDL_CreateWindow("Screensaver", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_RESIZABLE);
    SDL_MaximizeWindow(win);
    SDL_Renderer* R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    Particle* particles = NULL;
    DrawItem* drawbuf = NULL;

    if (mode == MODE_PARTICLES) {
        particles = malloc(sizeof(Particle) * N);
        drawbuf = malloc(sizeof(DrawItem) * N);
        init_particles(particles, N, 1234);
    }

    int running = 1;
    Uint32 last = SDL_GetTicks();
    float t = 0;
    int frame_count = 0;
    Uint32 fps_timer = SDL_GetTicks();
    int fps = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        t += dt;

        SDL_GetWindowSize(win, &W, &H);
        SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
        SDL_RenderClear(R);

        if (mode == MODE_PARTICLES) {
            update_particles(dt, particles, drawbuf, N, W, H, t, 6.0f, 4.0f);
            for (int i = 0; i < N; i++) {
                SDL_SetRenderDrawColor(R, drawbuf[i].r8, drawbuf[i].g8, drawbuf[i].b8, drawbuf[i].a8);
                SDL_FRect rect = {
                    drawbuf[i].x - drawbuf[i].r * 0.5f,
                    drawbuf[i].y - drawbuf[i].r * 0.5f,
                    drawbuf[i].r,
                    drawbuf[i].r
                };
                SDL_RenderFillRectF(R, &rect);
            }
        } else if (mode == MODE_CUBE3D) {
            render_cube3d(R, W, H, t);
        }

        SDL_RenderPresent(R);
        frame_count++;

        if (SDL_GetTicks() - fps_timer >= 1000) {
            fps = frame_count;
            frame_count = 0;
            fps_timer = SDL_GetTicks();
            char title[128];
            snprintf(title, sizeof(title), "Screensaver | FPS: %d", fps);
            SDL_SetWindowTitle(win, title);
        }
    }

    if (particles) free(particles);
    if (drawbuf) free(drawbuf);
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
