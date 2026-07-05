#ifndef HUD_SDL_H
#define HUD_SDL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "app_state.h"

void hud_render_sdl(SDL_Renderer* renderer, TTF_Font* font, AppCommonState* state,
                    int win_w, int win_h, int cpu_precision_128, uint32_t now);

#endif
