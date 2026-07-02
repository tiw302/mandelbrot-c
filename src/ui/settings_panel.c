/* settings_panel.c
 *
 * interactive settings overlay rendered via sokol_gl + fontstash.
 * provides real-time iteration slider, palette swatches, fractal type buttons,
 * and engine (CPU/GPU + perturbation) toggle buttons.
 * panel anchors to the right edge of the window; toggle with the I key.
 */

#include "settings_panel.h"

#include "color.h"
#include "config.h"
#include "ini_config.h"
#include "renderer.h"

/* sokol + fons are included by the translation unit that defines SOKOL_IMPL,
   but we need the types here — pull them in without re-defining the impl. */
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_gl.h"
#include "sokol/sokol_app.h"
#include "fons/fontstash.h"
#include "sokol/sokol_fontstash.h"

#include <stdio.h>
#include <math.h>

// -----------------------------------------------------------------------
// helper: draw a filled rectangle
// -----------------------------------------------------------------------
static void draw_rect(float x, float y, float w, float h,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    sgl_begin_quads();
    sgl_c4b(r, g, b, a);
    sgl_v2f(x,     y);
    sgl_v2f(x + w, y);
    sgl_v2f(x + w, y + h);
    sgl_v2f(x,     y + h);
    sgl_end();
}

// helper: draw a rectangle border only
static void draw_rect_border(float x, float y, float w, float h,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    sgl_begin_lines();
    sgl_c4b(r, g, b, a);
    sgl_v2f(x,     y);     sgl_v2f(x + w, y);
    sgl_v2f(x + w, y);     sgl_v2f(x + w, y + h);
    sgl_v2f(x + w, y + h); sgl_v2f(x,     y + h);
    sgl_v2f(x,     y + h); sgl_v2f(x,     y);
    sgl_end();
}

// -----------------------------------------------------------------------
// layout constants (all in screen pixels)
// -----------------------------------------------------------------------
#define PANEL_MARGIN      12.0f
#define SECTION_GAP       14.0f
#define LABEL_H           18.0f
#define SLIDER_H          20.0f
#define SWATCH_SIZE       22.0f
#define SWATCH_GAP         4.0f
#define SWATCH_PER_ROW     8
#define BTN_H             26.0f
#define BTN_GAP            5.0f

// -----------------------------------------------------------------------
// compute panel left edge from window width
// -----------------------------------------------------------------------
static float panel_x(int win_w) {
    return (float)win_w - SETTINGS_PANEL_WIDTH - 4.0f;
}

// -----------------------------------------------------------------------
// render
// -----------------------------------------------------------------------
void settings_panel_render(SettingsPanel* panel, FONScontext* fons, int font_id,
                            AppCommonState* state, int win_w, int win_h,
                            int gpu_mode, int use_perturbation,
                            sgl_pipeline pip_blend, uint32_t now) {
    (void)now;
    if (!panel->visible) return;

    const float px = panel_x(win_w);
    const float pw = SETTINGS_PANEL_WIDTH;
    float py = 8.0f;
    const float ph = (float)win_h - 16.0f;

    sgl_load_pipeline(pip_blend);

    // --- background ---
    draw_rect(px, py, pw, ph, 14, 14, 20, 235);
    draw_rect_border(px, py, pw, ph, 80, 80, 100, 200);

    // accent line on left edge
    draw_rect(px, py, 2.0f, ph, 100, 80, 220, 255);

    float cx = px + PANEL_MARGIN;
    float cy = py + PANEL_MARGIN;
    float inner_w = pw - PANEL_MARGIN * 2.0f;

    fonsClearState(fons);
    fonsSetFont(fons, font_id);
    fonsSetAlign(fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    // --- title ---
    fonsSetSize(fons, 14.0f);
    fonsSetColor(fons, sfons_rgba(160, 130, 255, 255));
    fonsDrawText(fons, cx, cy, "[ Settings ]", NULL);
    cy += LABEL_H + 4.0f;

    draw_rect(cx, cy, inner_w, 1.0f, 80, 80, 100, 180);
    cy += 8.0f;

    // ================================================================
    // SECTION 1 — Engine mode (CPU / GPU + Perturbation)
    // ================================================================
    fonsSetSize(fons, FONT_SIZE);
    fonsSetColor(fons, sfons_rgba(200, 200, 220, 255));
    fonsDrawText(fons, cx, cy, "Engine:", NULL);
    cy += LABEL_H + 2.0f;

    // CPU / GPU toggle — two half-width buttons side by side
    float half_w = (inner_w - BTN_GAP) / 2.0f;

    // CPU button
    int cpu_active = !gpu_mode;
    draw_rect(cx, cy, half_w, BTN_H,
              cpu_active ? 50 : 28, cpu_active ? 80 : 28, cpu_active ? 50 : 40, 230);
    draw_rect_border(cx, cy, half_w, BTN_H,
                     cpu_active ? 80 : 60, cpu_active ? 160 : 70, cpu_active ? 80 : 70, 200);
    fonsSetSize(fons, FONT_SIZE);
    fonsSetColor(fons, cpu_active ? sfons_rgba(100, 255, 120, 255) : sfons_rgba(160, 160, 180, 220));
    fonsDrawText(fons, cx + 6.0f, cy + 5.0f, "CPU", NULL);

    // GPU button
    float gx = cx + half_w + BTN_GAP;
    int gpu_active = gpu_mode;
    draw_rect(gx, cy, half_w, BTN_H,
              gpu_active ? 30 : 28, gpu_active ? 40 : 28, gpu_active ? 90 : 40, 230);
    draw_rect_border(gx, cy, half_w, BTN_H,
                     gpu_active ? 80 : 60, gpu_active ? 100 : 70, gpu_active ? 220 : 90, 200);
    fonsSetColor(fons, gpu_active ? sfons_rgba(120, 160, 255, 255) : sfons_rgba(160, 160, 180, 220));
    fonsDrawText(fons, gx + 6.0f, cy + 5.0f, "GPU", NULL);

    cy += BTN_H + BTN_GAP;

    // Perturbation toggle button (full width)
    int perturb_active = use_perturbation && gpu_mode;
    draw_rect(cx, cy, inner_w, BTN_H,
              perturb_active ? 60 : 28, perturb_active ? 40 : 28, perturb_active ? 100 : 40, 230);
    draw_rect_border(cx, cy, inner_w, BTN_H,
                     perturb_active ? 120 : 60, perturb_active ? 80 : 70, perturb_active ? 200 : 90, 200);
    fonsSetSize(fons, FONT_SIZE);
    fonsSetColor(fons, perturb_active ? sfons_rgba(180, 130, 255, 255) : sfons_rgba(140, 140, 160, 200));
    char perturb_label[48];
    if (!gpu_mode) {
        snprintf(perturb_label, sizeof(perturb_label), "Perturbation (GPU only)");
        fonsSetColor(fons, sfons_rgba(80, 80, 100, 160));
    } else {
        snprintf(perturb_label, sizeof(perturb_label), "Perturbation: %s", use_perturbation ? "ON" : "OFF");
    }
    fonsDrawText(fons, cx + 6.0f, cy + 5.0f, perturb_label, NULL);
    cy += BTN_H + SECTION_GAP;

    // ================================================================
    // SECTION 2 — Iterations slider
    // ================================================================
    draw_rect(cx, cy, inner_w, 1.0f, 80, 80, 100, 180);
    cy += 6.0f;

    char buf[64];
    snprintf(buf, sizeof(buf), "Iterations: %d", state->max_iterations);
    fonsSetSize(fons, FONT_SIZE);
    fonsSetColor(fons, sfons_rgba(200, 200, 220, 255));
    fonsDrawText(fons, cx, cy, buf, NULL);
    cy += LABEL_H + 2.0f;

    // track background
    float track_x = cx;
    float track_y = cy;
    float track_w = inner_w;
    draw_rect(track_x, track_y, track_w, SLIDER_H, 30, 30, 45, 255);
    draw_rect_border(track_x, track_y, track_w, SLIDER_H, 70, 70, 90, 200);

    // filled portion
    int max_limit = get_config_max_iterations_limit();
    if (max_limit < 1) max_limit = 10000;
    float frac = (float)state->max_iterations / (float)max_limit;
    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.0f) frac = 0.0f;
    float fill_w = frac * track_w;
    draw_rect(track_x, track_y, fill_w, SLIDER_H, 100, 70, 220, 220);

    // handle
    float handle_cx = track_x + fill_w;
    draw_rect(handle_cx - 4.0f, track_y - 2.0f, 8.0f, SLIDER_H + 4.0f, 200, 160, 255, 255);

    cy += SLIDER_H + SECTION_GAP;

    // ================================================================
    // SECTION 3 — Palette swatches
    // ================================================================
    draw_rect(cx, cy, inner_w, 1.0f, 80, 80, 100, 180);
    cy += 6.0f;

    fonsSetColor(fons, sfons_rgba(200, 200, 220, 255));
    fonsDrawText(fons, cx, cy, "Palette:", NULL);
    cy += LABEL_H + 2.0f;

    int palette_count = get_palette_count();
    if (palette_count < 1) palette_count = 1;

    int col = 0;
    float sw_x = cx;
    float sw_y = cy;
    for (int i = 0; i < palette_count; i++) {
        uint8_t sr = 80, sg = 80, sb = 200;
        if (i == state->palette_idx % palette_count) {
            uint32_t* lut = get_palette_lut();
            int lut_size = get_palette_lut_size();
            if (lut && lut_size > 0) {
                int sample = lut_size / 3;
                uint32_t c = lut[sample % lut_size];
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

        draw_rect(sw_x, sw_y, SWATCH_SIZE, SWATCH_SIZE, sr, sg, sb, 255);

        if (i == state->palette_idx % palette_count) {
            draw_rect_border(sw_x - 1, sw_y - 1, SWATCH_SIZE + 2, SWATCH_SIZE + 2, 255, 255, 255, 255);
        } else {
            draw_rect_border(sw_x, sw_y, SWATCH_SIZE, SWATCH_SIZE, 50, 50, 70, 180);
        }

        char idx_buf[4];
        snprintf(idx_buf, sizeof(idx_buf), "%d", i);
        fonsSetSize(fons, 10.0f);
        fonsSetColor(fons, sfons_rgba(255, 255, 255, 200));
        fonsDrawText(fons, sw_x + 3.0f, sw_y + 7.0f, idx_buf, NULL);

        col++;
        if (col >= SWATCH_PER_ROW) {
            col = 0;
            sw_x = cx;
            sw_y += SWATCH_SIZE + SWATCH_GAP;
        } else {
            sw_x += SWATCH_SIZE + SWATCH_GAP;
        }
    }
    int rows = (palette_count + SWATCH_PER_ROW - 1) / SWATCH_PER_ROW;
    cy += rows * (SWATCH_SIZE + SWATCH_GAP) + SECTION_GAP;

    // ================================================================
    // SECTION 4 — Fractal type buttons
    // ================================================================
    draw_rect(cx, cy, inner_w, 1.0f, 80, 80, 100, 180);
    cy += 6.0f;

    fonsSetSize(fons, FONT_SIZE);
    fonsSetColor(fons, sfons_rgba(200, 200, 220, 255));
    fonsDrawText(fons, cx, cy, "Fractal Type:", NULL);
    cy += LABEL_H + 2.0f;

    static const struct { const char* name; int mode; } FRACTAL_BTNS[] = {
        { "Mandelbrot",   0 },
        { "Burning Ship", 1 },
        { "Tricorn",      2 },
        { "Celtic",       3 },
        { "Buffalo",      4 },
    };
    int nb = (int)(sizeof(FRACTAL_BTNS) / sizeof(FRACTAL_BTNS[0]));
    for (int i = 0; i < nb; i++) {
        int is_active = (state->base_fractal == FRACTAL_BTNS[i].mode);
        draw_rect(cx, cy, inner_w, BTN_H,
                  is_active ? 80 : 28, is_active ? 50 : 28, is_active ? 180 : 40, 230);
        draw_rect_border(cx, cy, inner_w, BTN_H,
                         is_active ? 150 : 70, is_active ? 100 : 70, is_active ? 255 : 90, 200);
        fonsSetSize(fons, FONT_SIZE);
        fonsSetColor(fons, is_active ? sfons_rgba(220, 180, 255, 255) : sfons_rgba(180, 180, 200, 255));
        fonsDrawText(fons, cx + 8.0f, cy + 4.0f, FRACTAL_BTNS[i].name, NULL);
        cy += BTN_H + BTN_GAP;
    }

    // ================================================================
    // Footer hint
    // ================================================================
    cy = py + ph - LABEL_H - PANEL_MARGIN;
    fonsSetSize(fons, 12.0f);
    fonsSetColor(fons, sfons_rgba(100, 100, 120, 200));
    fonsDrawText(fons, cx, cy, "I - close  |  G - cpu/gpu  |  N - perturb", NULL);

    sfons_flush(fons);
}

// -----------------------------------------------------------------------
// geometry helpers — compute hit rects for each control
// -----------------------------------------------------------------------

// Returns left x and top y of the panel body content area
static void content_origin(int win_w, float* out_cx, float* out_cy) {
    *out_cx = panel_x(win_w) + PANEL_MARGIN;
    *out_cy = 8.0f + PANEL_MARGIN + LABEL_H + 4.0f + 1.0f + 8.0f; // after title + sep
}

// Returns y offset of the CPU button top (first thing after content_origin)
static float engine_section_y(int win_w) {
    float cx, cy;
    content_origin(win_w, &cx, &cy);
    (void)cx;
    return cy + LABEL_H + 2.0f; // after "Engine:" label
}

// Returns y offset of the perturbation button top
static float perturb_btn_y(int win_w) {
    return engine_section_y(win_w) + BTN_H + BTN_GAP;
}

static void slider_bounds(int win_w, float* out_x, float* out_y,
                           float* out_w, float* out_h) {
    float cx, base_y;
    content_origin(win_w, &cx, &base_y);
    // engine label + cpu/gpu buttons + perturb button + section gap + sep + 6 + iter label
    float cy = base_y + LABEL_H + 2.0f          // "Engine:" label
               + BTN_H + BTN_GAP                 // cpu/gpu row
               + BTN_H + SECTION_GAP             // perturb button
               + 1.0f + 6.0f                     // section separator
               + LABEL_H + 2.0f;                 // "Iterations: N" label
    *out_x = cx;
    *out_y = cy;
    *out_w = SETTINGS_PANEL_WIDTH - PANEL_MARGIN * 2.0f;
    *out_h = SLIDER_H;
}

static void swatch_bounds(int win_w, int idx, float* out_x, float* out_y) {
    float sx, sy, sw, sh;
    slider_bounds(win_w, &sx, &sy, &sw, &sh);
    float cy = sy + sh + SECTION_GAP + 1.0f + 6.0f + LABEL_H + 2.0f;
    int row = idx / SWATCH_PER_ROW;
    int col = idx % SWATCH_PER_ROW;
    *out_x = sx + col * (SWATCH_SIZE + SWATCH_GAP);
    *out_y = cy + row * (SWATCH_SIZE + SWATCH_GAP);
}

static void fractal_btn_bounds(int win_w, int palette_count, int btn_idx,
                                float* out_x, float* out_y, float* out_w, float* out_h) {
    float sx, sy, sw, sh;
    slider_bounds(win_w, &sx, &sy, &sw, &sh);
    int rows = (palette_count + SWATCH_PER_ROW - 1) / SWATCH_PER_ROW;
    float cy = sy + sh + SECTION_GAP
               + 1.0f + 6.0f + LABEL_H + 2.0f
               + rows * (SWATCH_SIZE + SWATCH_GAP) + SECTION_GAP
               + 1.0f + 6.0f + LABEL_H + 2.0f;
    *out_x = sx;
    *out_y = cy + btn_idx * (BTN_H + BTN_GAP);
    *out_w = sw;
    *out_h = BTN_H;
}

// -----------------------------------------------------------------------
// mouse event handlers
// -----------------------------------------------------------------------

SettingsMouseAction settings_panel_handle_mouse_down(SettingsPanel* panel,
                                                      AppCommonState* state,
                                                      int mx, int my,
                                                      int win_w, int win_h) {
    (void)win_h;
    if (!panel->visible) return SETTINGS_ACTION_NONE;

    // check if click is inside panel at all
    float px = panel_x(win_w);
    if ((float)mx < px) return SETTINGS_ACTION_NONE;

    float half_w = (SETTINGS_PANEL_WIDTH - PANEL_MARGIN * 2.0f - BTN_GAP) / 2.0f;
    float btn_x  = px + PANEL_MARGIN;
    float eng_y  = engine_section_y(win_w);

    // --- CPU button ---
    if ((float)mx >= btn_x && (float)mx <= btn_x + half_w &&
        (float)my >= eng_y && (float)my <= eng_y + BTN_H) {
        return SETTINGS_ACTION_TOGGLE_GPU; // backend will set gpu_mode = 0 (CPU)
    }

    // --- GPU button ---
    float gx = btn_x + half_w + BTN_GAP;
    if ((float)mx >= gx && (float)mx <= gx + half_w &&
        (float)my >= eng_y && (float)my <= eng_y + BTN_H) {
        return SETTINGS_ACTION_TOGGLE_GPU; // backend will set gpu_mode = 1 (GPU)
    }

    // --- Perturbation button ---
    float pb_y = perturb_btn_y(win_w);
    if ((float)mx >= btn_x && (float)mx <= btn_x + (SETTINGS_PANEL_WIDTH - PANEL_MARGIN * 2.0f) &&
        (float)my >= pb_y && (float)my <= pb_y + BTN_H) {
        return SETTINGS_ACTION_TOGGLE_PERTURB;
    }

    // --- Iterations slider ---
    float sx, sy, sw, sh;
    slider_bounds(win_w, &sx, &sy, &sw, &sh);
    if ((float)mx >= sx && (float)mx <= sx + sw &&
        (float)my >= sy && (float)my <= sy + sh) {
        float frac = ((float)mx - sx) / sw;
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
        return SETTINGS_ACTION_CONSUMED;
    }

    // --- Palette swatches ---
    int palette_count = get_palette_count();
    if (palette_count < 1) palette_count = 1;
    for (int i = 0; i < palette_count; i++) {
        float swx, swy;
        swatch_bounds(win_w, i, &swx, &swy);
        if ((float)mx >= swx && (float)mx <= swx + SWATCH_SIZE &&
            (float)my >= swy && (float)my <= swy + SWATCH_SIZE) {
            state->palette_idx = i;
            init_color_palette(state->max_iterations, state->palette_idx);
            state->needs_redraw = 1;
            return SETTINGS_ACTION_CONSUMED;
        }
    }

    // --- Fractal type buttons ---
    static const int FRACTAL_MODES[] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++) {
        float bx, by, bw, bh;
        fractal_btn_bounds(win_w, palette_count, i, &bx, &by, &bw, &bh);
        if ((float)mx >= bx && (float)mx <= bx + bw &&
            (float)my >= by && (float)my <= by + bh) {
            state->base_fractal = FRACTAL_MODES[i];
            state->needs_redraw = 1;
            return SETTINGS_ACTION_CONSUMED;
        }
    }

    // click inside panel but no control — consume to block camera pan
    return SETTINGS_ACTION_CONSUMED;
}

int settings_panel_handle_mouse_move(SettingsPanel* panel, AppCommonState* state, int mx) {
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

int settings_panel_handle_mouse_up(SettingsPanel* panel, AppCommonState* state) {
    (void)state;
    if (!panel->visible || !panel->drag_active) return 0;
    panel->drag_active = 0;
    return 1;
}
