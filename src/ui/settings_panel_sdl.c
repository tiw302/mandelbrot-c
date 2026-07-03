/* settings_panel_sdl.c
 *
 * interactive settings panel for the SDL2 CPU-mode renderer.
 * uses SDL_RenderFillRect for boxes and TTF_Font for text — no new deps.
 * provides: iterations slider, palette swatches, fractal type buttons,
 *           precision toggle, thread count +/- buttons.
 */

#include "settings_panel_sdl.h"
#include "color.h"
#include "config.h"
#include "ini_config.h"

#include <stdio.h>
#include <math.h>

// -----------------------------------------------------------------------
// helpers for SDL drawing
// -----------------------------------------------------------------------
static void sdl_fill(SDL_Renderer* r, int x, int y, int w, int h,
                     uint8_t rv, uint8_t g, uint8_t b, uint8_t a) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, rv, g, b, a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void sdl_border(SDL_Renderer* r, int x, int y, int w, int h,
                       uint8_t rv, uint8_t g, uint8_t b, uint8_t a) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, rv, g, b, a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}

static void sdl_text(SDL_Renderer* r, TTF_Font* font,
                     const char* text, int x, int y,
                     uint8_t rv, uint8_t g, uint8_t b, uint8_t a) {
    SDL_Color col = {rv, g, b, a};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

// -----------------------------------------------------------------------
// layout constants
// -----------------------------------------------------------------------
#define PM   12   // panel margin
#define LH   20   // line height
#define SLH  20   // slider height
#define SWS  22   // swatch size
#define SWG   4   // swatch gap
#define SPR   8   // swatches per row
#define BH   26   // button height
#define BG    5   // button gap
#define SEP   6   // separator gap

static int panel_x(int win_w) { return win_w - SDL_PANEL_WIDTH - 4; }

// -----------------------------------------------------------------------
// render
// -----------------------------------------------------------------------
void settings_panel_sdl_render(SettingsPanelSdl* panel, SDL_Renderer* renderer,
                                TTF_Font* font, AppCommonState* state,
                                int win_w, int win_h,
                                int cpu_precision_128, int thread_count,
                                uint32_t now) {
    (void)now;
    if (!panel->visible || !renderer || !font) return;

    int px = panel_x(win_w);
    int pw = SDL_PANEL_WIDTH;
    int py = 8;
    int ph = win_h - 16;
    int inner_w = pw - PM * 2;
    int cx = px + PM;
    int cy = py + PM;

    // background + border + left accent
    sdl_fill(renderer,   px,     py, pw,  ph, 14, 14, 20, 235);
    sdl_border(renderer, px,     py, pw,  ph, 80, 80,100, 200);
    sdl_fill(renderer,   px,     py, 2,   ph,100, 80,220, 255);

    // title
    sdl_text(renderer, font, "[ Settings ]", cx, cy, 160, 130, 255, 255);
    cy += LH + 4;
    sdl_fill(renderer, cx, cy, inner_w, 1, 80, 80, 100, 180);
    cy += SEP;

    // ================================================================
    // SECTION 1 — Precision + Threads (CPU-specific)
    // ================================================================
    sdl_text(renderer, font, "CPU Options:", cx, cy, 200, 200, 220, 255);
    cy += LH + 2;

    // Precision button (full width)
    const char* prec_label = cpu_precision_128 ? "Precision: 128-bit (SIMD)" : "Precision: 64-bit (Double)";
    sdl_fill(renderer,   cx, cy, inner_w, BH,
             cpu_precision_128 ? 30 : 28, cpu_precision_128 ? 60 : 28,
             cpu_precision_128 ? 80 : 40, 230);
    sdl_border(renderer, cx, cy, inner_w, BH,
               cpu_precision_128 ? 60 : 60, cpu_precision_128 ? 130 : 70,
               cpu_precision_128 ? 150 : 90, 200);
    sdl_text(renderer, font, prec_label, cx + 6, cy + 4,
             cpu_precision_128 ? 100 : 180, cpu_precision_128 ? 200 : 180,
             cpu_precision_128 ? 255 : 200, 255);
    cy += BH + BG;

    // Threads: [−] N [+]
    char thread_buf[32];
    snprintf(thread_buf, sizeof(thread_buf), "Threads: %d", thread_count);
    sdl_text(renderer, font, thread_buf, cx, cy, 200, 200, 220, 255);
    cy += LH + 2;

    int btn_w = 28;
    // [-] button
    sdl_fill(renderer,   cx, cy, btn_w, BH, 50, 30, 30, 230);
    sdl_border(renderer, cx, cy, btn_w, BH, 120, 70, 70, 200);
    sdl_text(renderer, font, "-", cx + 9, cy + 3, 255, 160, 160, 255);
    // [+] button
    int plus_x = cx + inner_w - btn_w;
    sdl_fill(renderer,   plus_x, cy, btn_w, BH, 30, 50, 30, 230);
    sdl_border(renderer, plus_x, cy, btn_w, BH, 70, 120, 70, 200);
    sdl_text(renderer, font, "+", plus_x + 7, cy + 3, 160, 255, 160, 255);
    // thread bar fill
    int max_threads = 32;
    int bar_x = cx + btn_w + 4;
    int bar_w = inner_w - btn_w * 2 - 8;
    sdl_fill(renderer, bar_x, cy, bar_w, BH, 30, 30, 45, 255);
    int fill = (thread_count > max_threads ? max_threads : thread_count) * bar_w / max_threads;
    sdl_fill(renderer, bar_x, cy, fill, BH, 60, 140, 60, 200);
    sdl_border(renderer, bar_x, cy, bar_w, BH, 60, 80, 60, 150);
    cy += BH + BG + SEP;

    // ================================================================
    // SECTION 2 — Iterations slider
    // ================================================================
    sdl_fill(renderer, cx, cy, inner_w, 1, 80, 80, 100, 180);
    cy += SEP;

    char iter_buf[40];
    snprintf(iter_buf, sizeof(iter_buf), "Iterations: %d", state->max_iterations);
    sdl_text(renderer, font, iter_buf, cx, cy, 200, 200, 220, 255);
    cy += LH + 2;

    int max_limit = get_config_max_iterations_limit();
    if (max_limit < 1) max_limit = 10000;
    float frac = (float)state->max_iterations / (float)max_limit;
    if (frac > 1.0f) frac = 1.0f;
    int fill_w = (int)(frac * inner_w);

    sdl_fill(renderer,   cx, cy, inner_w, SLH, 30, 30, 45, 255);
    sdl_fill(renderer,   cx, cy, fill_w,  SLH, 100, 70, 220, 220);
    sdl_border(renderer, cx, cy, inner_w, SLH, 70, 70, 90, 200);
    // handle
    sdl_fill(renderer, cx + fill_w - 4, cy - 2, 8, SLH + 4, 200, 160, 255, 255);
    cy += SLH + SEP;

    // ================================================================
    // SECTION 3 — Palette swatches
    // ================================================================
    sdl_fill(renderer, cx, cy, inner_w, 1, 80, 80, 100, 180);
    cy += SEP;
    sdl_text(renderer, font, "Palette:", cx, cy, 200, 200, 220, 255);
    cy += LH + 2;

    int palette_count = get_palette_count();
    if (palette_count < 1) palette_count = 1;

    int col = 0;
    int sw_x = cx, sw_y = cy;
    for (int i = 0; i < palette_count; i++) {
        uint8_t sr = 80, sg = 80, sb = 200;
        if (i == state->palette_idx % palette_count) {
            uint32_t* lut = get_palette_lut();
            int lut_size = get_palette_lut_size();
            if (lut && lut_size > 0) {
                uint32_t c = lut[(lut_size / 3) % lut_size];
                sr = (uint8_t)((c >> 16) & 0xFF);
                sg = (uint8_t)((c >> 8)  & 0xFF);
                sb = (uint8_t)( c        & 0xFF);
            }
        } else {
            float hue = (float)i / (float)palette_count;
            sr = (uint8_t)(128 + 100 * sinf(hue * 6.28f));
            sg = (uint8_t)(80  + 80  * sinf(hue * 6.28f + 2.09f));
            sb = (uint8_t)(128 + 100 * cosf(hue * 6.28f));
        }

        sdl_fill(renderer, sw_x, sw_y, SWS, SWS, sr, sg, sb, 255);
        if (i == state->palette_idx % palette_count) {
            sdl_border(renderer, sw_x - 1, sw_y - 1, SWS + 2, SWS + 2, 255, 255, 255, 255);
        } else {
            sdl_border(renderer, sw_x, sw_y, SWS, SWS, 50, 50, 70, 180);
        }

        char idx_buf[4];
        snprintf(idx_buf, sizeof(idx_buf), "%d", i);
        // render tiny index label
        sdl_text(renderer, font, idx_buf, sw_x + 3, sw_y + 5, 255, 255, 255, 200);

        col++;
        if (col >= SPR) { col = 0; sw_x = cx; sw_y += SWS + SWG; }
        else sw_x += SWS + SWG;
    }
    int rows = (palette_count + SPR - 1) / SPR;
    cy += rows * (SWS + SWG) + SEP;

    // ================================================================
    // SECTION 4 — Fractal type buttons
    // ================================================================
    sdl_fill(renderer, cx, cy, inner_w, 1, 80, 80, 100, 180);
    cy += SEP;
    sdl_text(renderer, font, "Fractal Type:", cx, cy, 200, 200, 220, 255);
    cy += LH + 2;

    static const struct { const char* name; int mode; } FBTNS[] = {
        {"Mandelbrot",   0}, {"Burning Ship", 1},
        {"Tricorn",      2}, {"Celtic",       3}, {"Buffalo", 4},
    };
    for (int i = 0; i < 5; i++) {
        int active = (state->base_fractal == FBTNS[i].mode);
        sdl_fill(renderer,   cx, cy, inner_w, BH,
                 active ? 80 : 28, active ? 50 : 28, active ? 180 : 40, 230);
        sdl_border(renderer, cx, cy, inner_w, BH,
                   active ? 150 : 70, active ? 100 : 70, active ? 255 : 90, 200);
        sdl_text(renderer, font, FBTNS[i].name, cx + 8, cy + 4,
                 active ? 220 : 180, active ? 180 : 180, active ? 255 : 200, 255);
        cy += BH + BG;
    }

    // footer
    cy = py + ph - LH - PM;
    sdl_text(renderer, font, "I - close  |  E - precision  |  [/] - threads",
             cx, cy, 100, 100, 120, 200);
}

// -----------------------------------------------------------------------
// geometry helpers (parallel to GPU panel logic)
// -----------------------------------------------------------------------
static int get_precision_btn_y(void)  { return 8 + PM + LH + 4 + SEP + LH + 2; }
static int get_threads_minus_y(void)  { return get_precision_btn_y() + BH + BG + LH + 2; }
static int get_slider_y(int win_w) {
    (void)win_w;
    return get_threads_minus_y() + BH + BG + SEP + SEP + LH + 2;
}

static void get_swatch_pos(int win_w, int idx, int* ox, int* oy) {
    int cx = panel_x(win_w) + PM;
    int sy = get_slider_y(win_w) + SLH + SEP + SEP + LH + 2;
    *ox = cx + (idx % SPR) * (SWS + SWG);
    *oy = sy + (idx / SPR) * (SWS + SWG);
}

static int get_fractal_btn_y(int win_w, int palette_count, int btn_idx) {
    int cx = panel_x(win_w) + PM;
    (void)cx;
    int rows = (palette_count + SPR - 1) / SPR;
    int base = get_slider_y(win_w) + SLH + SEP + SEP + LH + 2
               + rows * (SWS + SWG) + SEP + SEP + LH + 2;
    return base + btn_idx * (BH + BG);
}

// -----------------------------------------------------------------------
// mouse handlers
// -----------------------------------------------------------------------
SdlPanelAction settings_panel_sdl_mouse_down(SettingsPanelSdl* panel, AppCommonState* state,
                                              int mx, int my, int win_w, int win_h) {
    (void)win_h;
    if (!panel->visible) return SDL_PANEL_ACTION_NONE;

    int px = panel_x(win_w);
    if (mx < px) return SDL_PANEL_ACTION_NONE;

    int cx = px + PM;
    int inner_w = SDL_PANEL_WIDTH - PM * 2;

    // precision button
    int prec_y = get_precision_btn_y();
    if (mx >= cx && mx <= cx + inner_w && my >= prec_y && my <= prec_y + BH)
        return SDL_PANEL_ACTION_TOGGLE_PRECISION;

    // threads [-] button
    int th_y = get_threads_minus_y();
    if (mx >= cx && mx <= cx + 28 && my >= th_y && my <= th_y + BH)
        return SDL_PANEL_ACTION_THREADS_DOWN;

    // threads [+] button
    int plus_x = cx + inner_w - 28;
    if (mx >= plus_x && mx <= plus_x + 28 && my >= th_y && my <= th_y + BH)
        return SDL_PANEL_ACTION_THREADS_UP;

    // iterations slider
    int sl_y = get_slider_y(win_w);
    if (mx >= cx && mx <= cx + inner_w && my >= sl_y && my <= sl_y + SLH) {
        float frac = (float)(mx - cx) / (float)inner_w;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        int limit = get_config_max_iterations_limit();
        if (limit < 1) limit = 10000;
        int new_iter = (int)(frac * (float)limit);
        if (new_iter < 10) new_iter = 10;
        state->max_iterations = new_iter;
        init_color_palette(state->max_iterations, state->palette_idx);
        state->needs_redraw = 1;

        panel->drag_active     = 1;
        panel->drag_start_x    = mx;
        panel->drag_start_iter = new_iter;
        return SDL_PANEL_ACTION_CONSUMED;
    }

    // palette swatches
    int palette_count = get_palette_count();
    if (palette_count < 1) palette_count = 1;
    for (int i = 0; i < palette_count; i++) {
        int swx, swy;
        get_swatch_pos(win_w, i, &swx, &swy);
        if (mx >= swx && mx <= swx + SWS && my >= swy && my <= swy + SWS) {
            state->palette_idx = i;
            init_color_palette(state->max_iterations, state->palette_idx);
            state->needs_redraw = 1;
            return SDL_PANEL_ACTION_CONSUMED;
        }
    }

    // fractal buttons
    static const int FMODES[] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++) {
        int fy = get_fractal_btn_y(win_w, palette_count, i);
        if (mx >= cx && mx <= cx + inner_w && my >= fy && my <= fy + BH) {
            state->base_fractal = FMODES[i];
            state->needs_redraw = 1;
            return SDL_PANEL_ACTION_CONSUMED;
        }
    }

    return SDL_PANEL_ACTION_CONSUMED; // consume all clicks inside panel
}

int settings_panel_sdl_mouse_move(SettingsPanelSdl* panel, AppCommonState* state, int mx) {
    if (!panel->visible || !panel->drag_active) return 0;

    int limit = get_config_max_iterations_limit();
    if (limit < 1) limit = 10000;

    int delta = mx - panel->drag_start_x;
    float sensitivity = (float)limit / 300.0f;
    int new_iter = panel->drag_start_iter + (int)((float)delta * sensitivity);
    if (new_iter < 10) new_iter = 10;
    if (new_iter > limit) new_iter = limit;

    state->max_iterations = new_iter;
    init_color_palette(state->max_iterations, state->palette_idx);
    state->needs_redraw = 1;
    return 1;
}

int settings_panel_sdl_mouse_up(SettingsPanelSdl* panel, AppCommonState* state) {
    (void)state;
    if (!panel->visible || !panel->drag_active) return 0;
    panel->drag_active = 0;
    return 1;
}
