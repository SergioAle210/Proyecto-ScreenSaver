// Screensaver 4D — C (C11) + SDL2 + OpenMP
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "sim.h"
#include "tesseract.h"

#ifdef _OPENMP
  #include <omp.h>
#endif

#include <SDL2/SDL.h>

enum Mode { MODE_PARTICLES=0, MODE_TESSERACT=1 };

static void set_window_title_fps(SDL_Window* win, float fps, int mode, const Camera3* cam) {
    char buf[256];
    if (mode==MODE_TESSERACT)
        snprintf(buf, sizeof(buf),
            "Screensaver 4D (C) [tesseract]  |  FPS: %.1f  |  FOV: %.0f  |  pos(%.1f,%.1f,%.1f)",
            fps, cam->fov_deg, cam->px, cam->py, cam->pz);
    else
        snprintf(buf, sizeof(buf), "Screensaver 4D (C) [particles]  |  FPS: %.1f", fps);
    SDL_SetWindowTitle(win, buf);
}

static int parse_args(int argc, char** argv, int* N, int* width, int* height,
                      int* threads, unsigned int* seed, int* fpscap, int* mode)
{
    if (argc < 2) return 0;
    *N = atoi(argv[1]); if (*N < 1) *N = 1;
    *width = 1280; *height = 720;
    *threads = -1; *seed = 12345u; *fpscap = 0; *mode = MODE_PARTICLES;

    if (argc >= 4) {
        *width  = atoi(argv[2]); if (*width  < 640) *width  = 640;
        *height = atoi(argv[3]); if (*height < 480) *height = 480;
    }
    for (int i=4; i<argc; ++i) {
        if (strcmp(argv[i], "--threads") == 0 && i+1 < argc) { *threads = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--seed") == 0 && i+1 < argc) { *seed = (unsigned int)strtoul(argv[++i], NULL, 10); }
        else if (strcmp(argv[i], "--fpscap") == 0 && i+1 < argc) { *fpscap = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--mode") == 0 && i+1 < argc) {
            ++i;
            if (strcmp(argv[i], "tesseract")==0) *mode = MODE_TESSERACT;
            else *mode = MODE_PARTICLES;
        }
    }
    return 1;
}

int main(int argc, char** argv)
{
    int N=0, W=1280, H=720, threads=-1, fpscap=0, mode=MODE_PARTICLES;
    unsigned int seed=12345u;
    if (!parse_args(argc, argv, &N, &W, &H, &threads, &seed, &fpscap, &mode)) {
        printf("Uso: %s N [width height] [--seed S] [--fpscap X] [--threads T] [--mode particles|tesseract]\n", argv[0]);
        return 1;
    }

#ifdef _OPENMP
    if (threads > 0) omp_set_num_threads(threads);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Screensaver 4D (C)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }

    // Estado de partículas (solo se usa en MODE_PARTICLES)
    Particle* particles = (Particle*)malloc(sizeof(Particle) * (size_t)N);
    DrawItem* drawbuf   = (DrawItem*)malloc(sizeof(DrawItem) * (size_t)N);
    if (!particles || !drawbuf) {
        fprintf(stderr, "Error: memoria insuficiente\n");
        free(particles); free(drawbuf);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }
    init_particles(particles, N, seed);

    // Cámara 3D para tesseracto
    Camera3 cam = {0.0f, 0.0f, -5.0f, 0.0f, 0.0f, 70.0f};
    bool mouse_captured = false;

    const float focal4 = 2.0f; // perspectiva 4D
    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_prev = SDL_GetPerformanceCounter();

    float fps_smoothed = 0.0f;
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = 0;
                if (e.key.keysym.sym == SDLK_t) mode = (mode==MODE_PARTICLES? MODE_TESSERACT: MODE_PARTICLES);
                if (e.key.keysym.sym == SDLK_m) { // capturar/soltar ratón
                    mouse_captured = !mouse_captured;
                    SDL_SetRelativeMouseMode(mouse_captured ? SDL_TRUE : SDL_FALSE);
                }
                if (e.key.keysym.sym == SDLK_r) { // reset cámara
                    cam.px = cam.py = 0.0f; cam.pz = -5.0f; cam.yaw = cam.pitch = 0.0f; cam.fov_deg = 70.0f;
                }
            }
            if (e.type == SDL_MOUSEMOTION && mouse_captured) {
                const float sens = 0.0025f;
                cam.yaw   += e.motion.xrel * sens;
                cam.pitch += e.motion.yrel * sens;
                if (cam.pitch >  1.55f) cam.pitch =  1.55f;
                if (cam.pitch < -1.55f) cam.pitch = -1.55f;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                cam.fov_deg += (e.wheel.y < 0 ? +3.0f : -3.0f);
                if (cam.fov_deg < 30.0f) cam.fov_deg = 30.0f;
                if (cam.fov_deg > 110.0f) cam.fov_deg = 110.0f;
            }
        }

        // delta tiempo
        Uint64 t_now = SDL_GetPerformanceCounter();
        double dt = (double)(t_now - t_prev) / (double)perf_freq;
        t_prev = t_now; if (dt > 0.1) dt = 0.1;
        float t = (float)(SDL_GetTicks() * 0.001f);

        // Movimiento WASD + QE
        const Uint8* kb = SDL_GetKeyboardState(NULL);
        float spd = 3.0f; if (kb[SDL_SCANCODE_LSHIFT]) spd *= 3.0f;
        float move = spd * (float)dt;
        float cyaw = cosf(cam.yaw), syaw = sinf(cam.yaw);
        float fwdx =  syaw, fwdz = cyaw;
        float rgtx =  cyaw, rgtz = -syaw;

        if (kb[SDL_SCANCODE_W]) { cam.px += fwdx*move; cam.pz += fwdz*move; }
        if (kb[SDL_SCANCODE_S]) { cam.px -= fwdx*move; cam.pz -= fwdz*move; }
        if (kb[SDL_SCANCODE_A]) { cam.px -= rgtx*move; cam.pz -= rgtz*move; }
        if (kb[SDL_SCANCODE_D]) { cam.px += rgtx*move; cam.pz += rgtz*move; }
        if (kb[SDL_SCANCODE_Q]) { cam.py -= move; }
        if (kb[SDL_SCANCODE_E]) { cam.py += move; }

        // clear
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 8, 13, 255);
        SDL_RenderClear(renderer);

        if (mode == MODE_PARTICLES) {
            const float focal3 = 2.0f;
            update_particles((float)dt, particles, drawbuf, N, W, H, t, focal4, focal3);
            for (int i=0; i<N; ++i) {
                SDL_SetRenderDrawColor(renderer, drawbuf[i].r8, drawbuf[i].g8, drawbuf[i].b8, drawbuf[i].a8);
                SDL_FRect rect = { drawbuf[i].x - drawbuf[i].r*0.5f,
                                   drawbuf[i].y - drawbuf[i].r*0.5f,
                                   drawbuf[i].r, drawbuf[i].r };
                SDL_RenderFillRectF(renderer, &rect);
            }
        } else {
            render_tesseract(renderer, W, H, t, focal4, &cam);
        }
        SDL_RenderPresent(renderer);

        // FPS (0.5 s de ventana)
        static double acc=0; static int frames=0; acc+=dt; frames++;
        if (acc >= 0.5) {
            float fps = (float)(frames/acc);
            fps_smoothed = 0.7f*fps_smoothed + 0.3f*fps;
            set_window_title_fps(window, fps_smoothed, mode, &cam);
            acc=0; frames=0;
        }

        // Cap de FPS (opcional)
        if (fpscap > 0) {
            static Uint32 last = 0;
            Uint32 now = SDL_GetTicks();
            Uint32 target_ms = (Uint32)((1000.0f/(float)fpscap)+0.5f);
            if (now - last < target_ms) SDL_Delay(target_ms - (now - last));
            last = SDL_GetTicks();
        }
    }

    free(particles); free(drawbuf);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
