#include "cloth.h"
#include <SDL2/SDL.h>

void cloth_render_seq(SDL_Renderer *R, const ClothState *S)
{
    const int N = S->N;
    for (int q = 0; q < N; ++q)
    {
        const DrawItem *d = &S->draw[S->order_idx[q]];
        SDL_SetTextureColorMod(S->sprite, d->r8, d->g8, d->b8);
        SDL_SetTextureAlphaMod(S->sprite, d->a8);
        float diam = d->r * 2.0f;
        SDL_FRect dst = {(d->x + S->tx) - d->r, (d->y + S->ty) - d->r, diam, diam};
        SDL_RenderCopyF(R, S->sprite, NULL, &dst);
    }
}
