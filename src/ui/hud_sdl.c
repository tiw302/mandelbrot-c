/* hud_sdl.c
 *
 * text and overlay rendering for the sdl2 cpu-mode window.
 * draws telemetry, active status, and mega screenshot progress bars.
 */

#include "hud_sdl.h"
#include "color.h"
#include "config.h"
#include "renderer.h"
#include "screenshot.h"
#include "fractal.h"
#include <stdio.h>

static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y,
                        SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surface);
}

void hud_render_sdl(SDL_Renderer* renderer, TTF_Font* font, AppCommonState* state, int win_w,
                    int win_h, int cpu_precision_128, uint32_t now) {
    if (!font) return;

    char buf[256];
    SDL_Color white = {255, 255, 255, 255};
    int x = 15, y = 12, line_h = FONT_SIZE + 6;

    if (state->show_help) {
        float w = 560.0f;
        float h = 380.0f;
        float rx = (win_w - w) / 2.0f;
        float ry = (win_h - h) / 2.0f;

        SDL_Rect bg = {(int)rx, (int)ry, (int)w, (int)h};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 245);
        SDL_RenderFillRect(renderer, &bg);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &bg);

        SDL_Color title_color = {255, 60, 60, 255};
        int tx = (int)rx + 20;
        int ty = (int)ry + 20;

        render_text(renderer, font, "[ Keyboard Controls Guide ]", tx, ty, title_color);
        ty += line_h * 2;

        render_text(renderer, font, "H / F1       : Toggle this Help Menu", tx, ty, white); ty += line_h;
        render_text(renderer, font, "ESC / Q      : Quit Application", tx, ty, white); ty += line_h;
        render_text(renderer, font, "Ctrl + Z     : Undo Camera Zoom/Pan", tx, ty, white); ty += line_h;
        render_text(renderer, font, "R            : Reset View", tx, ty, white); ty += line_h;
        render_text(renderer, font, "P / 0-9      : Cycle / Select Color Palette", tx, ty, white); ty += line_h;
        render_text(renderer, font, "UP / DOWN    : Adjust Iterations (Shift x10)", tx, ty, white); ty += line_h;
        render_text(renderer, font, "[ / ]        : Decrease / Increase CPU Threads", tx, ty, white); ty += line_h;
        render_text(renderer, font, "E            : Toggle 128-bit CPU Precision", tx, ty, white); ty += line_h;
        render_text(renderer, font, "J            : Toggle Julia Explorer Mode", tx, ty, white); ty += line_h;
        render_text(renderer, font, "B            : Toggle Burning Ship Mode", tx, ty, white); ty += line_h;
        render_text(renderer, font, "S            : Capture Screenshot", tx, ty, white); ty += line_h;
        render_text(renderer, font, "V            : Start / Stop Video Recording", tx, ty, white);
        return;
    }

    SDL_Rect bg = {5, 5, 700, 3 * line_h + 20};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 220);
    SDL_RenderFillRect(renderer, &bg);

    const char* engine_type = cpu_precision_128 ? "CPU (128-bit)" : "CPU (64-bit)";
    const FractalDefinition* fd = get_fractal_by_mode(state->base_fractal);
    const char* mode_name = state->julia_mode
                                ? "Julia"
                                : (fd ? fd->display_name : "Unknown");

    snprintf(buf, sizeof(buf), "[ENGINE] %s | Mode: %s | Threads: %d | Render: %u ms", engine_type,
             mode_name, state->thread_count, state->render_time_ms);
    render_text(renderer, font, buf, x, y, white);
    y += line_h;

    if (state->julia_mode)
        snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", state->julia_c.re,
                 state->julia_c.im);
    else
        snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)",
                 (double)state->cam.view.center_re, (double)state->cam.view.center_im);
    render_text(renderer, font, buf, x, y, white);
    y += line_h;

    snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
             (double)state->cam.view.zoom, state->max_iterations,
             get_palette_name(state->palette_idx % get_palette_count()));
    render_text(renderer, font, buf, x, y, white);

    // draw stacked notifications overlay in the bottom right corner
    int draw_count = 0;
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            uint32_t elapsed = now - state->notifications[i].start_time;
            if (elapsed > 3000) {
                /* intentional side-effect: expire notification here since there is
                 * no separate notification-update tick in the render loop. */
                state->notifications[i].active = 0;
            } else {
                float tw = 260.0f;
                float th = 40.0f;
                float trx = (float)win_w - tw - 20.0f;
                float try = (float)win_h - th - 20.0f - (float)draw_count * (th + 10.0f);

                SDL_Rect t_bg = {(int)trx, (int)try, (int)tw, (int)th};
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 15, 15, 20, 230);
                SDL_RenderFillRect(renderer, &t_bg);

                SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
                SDL_RenderDrawRect(renderer, &t_bg);

                int tx = (int)trx + 20;
                int ty = (int)try + 10;
                render_text(renderer, font, state->notifications[i].message, tx, ty, white);

                draw_count++;
            }
        }
    }

    // draw mega screenshot progress overlay in the bottom left corner if active
    if (state->mega_screenshot_active == 1) {
        float tw = 260.0f;
        float th = 40.0f;
        float trx = 20.0f;
        float try = (float)win_h - th - 20.0f;

        SDL_Rect t_bg = {(int)trx, (int)try, (int)tw, (int)th};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 230);
        SDL_RenderFillRect(renderer, &t_bg);

        SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
        SDL_RenderDrawRect(renderer, &t_bg);

        float pb_w = (tw - 10.0f) * (float)state->mega_screenshot_progress / 100.0f;
        SDL_Rect t_pb = {(int)trx + 5, (int)try + (int)th - 6, (int)pb_w, 3};
        SDL_SetRenderDrawColor(renderer, 40, 200, 40, 255);
        SDL_RenderFillRect(renderer, &t_pb);

        char progress_msg[64];
        snprintf(progress_msg, sizeof(progress_msg), "Generating 8K: %d%%", state->mega_screenshot_progress);
        int tx = (int)trx + 20;
        int ty = (int)try + 10;
        render_text(renderer, font, progress_msg, tx, ty, white);
    }
}
