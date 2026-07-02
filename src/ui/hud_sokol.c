/* hud_sokol.c
 *
 * telemetry and notification overlay rendering for sokol pipelines.
 * renders text using fontstash and processes mouse events.
 */

#include "hud_sokol.h"
#include "color.h"
#include "config.h"
#include "ini_config.h"
#include "renderer.h"
#include "screenshot.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_gl.h"
#include "fons/fontstash.h"
#include "sokol/sokol_fontstash.h"
#include "sokol/sokol_app.h"
#include "tour.h"
#include <stdio.h>

void hud_render_sokol_gpu(FONScontext* fons, int font_id, AppCommonState* state,
                          int win_w, int win_h, int gpu_mode, int high_precision_mode,
                          int cpu_precision_128, int active_perturbation_last,
                          int use_perturbation, sgl_pipeline pip_blend, uint32_t now) {
    float visual_font_size = FONT_SIZE;
    float lh = visual_font_size + 6.0f;

    if (state->show_help) {
        sgl_load_pipeline(pip_blend);
        sgl_begin_quads();
        float w = 580.0f;
        float h = 420.0f;
        float rx = (win_w - w) / 2.0f;
        float ry = (win_h - h) / 2.0f;

        sgl_c4b(15, 15, 20, 245);
        sgl_v2f(rx, ry);
        sgl_v2f(rx + w, ry);
        sgl_v2f(rx + w, ry + h);
        sgl_v2f(rx, ry + h);
        sgl_end();

        sgl_begin_lines();
        sgl_c4b(255, 0, 0, 255);
        sgl_v2f(rx, ry); sgl_v2f(rx + w, ry);
        sgl_v2f(rx + w, ry); sgl_v2f(rx + w, ry + h);
        sgl_v2f(rx + w, ry + h); sgl_v2f(rx, ry + h);
        sgl_v2f(rx, ry + h); sgl_v2f(rx, ry);
        sgl_end();

        fonsClearState(fons);
        fonsSetFont(fons, font_id);
        fonsSetSize(fons, visual_font_size);
        fonsSetAlign(fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        float tx = rx + 25.0f;
        float ty = ry + 20.0f;

        fonsSetColor(fons, sfons_rgba(255, 60, 60, 255));
        fonsDrawText(fons, tx, ty, "[ Keyboard Controls Guide ]", NULL);
        ty += lh * 2.0f;

        fonsSetColor(fons, sfons_rgba(255, 255, 255, 255));
        fonsDrawText(fons, tx, ty, "H / F5       : Toggle Help / Reload Shaders", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "ESC / Q      : Quit Application", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "Ctrl + Z     : Undo Camera Zoom/Pan", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "R            : Reset View", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "P / 0-9      : Cycle / Select Color Palette", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "UP / DOWN    : Adjust Iterations (Shift x10)", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "G            : Toggle CPU <-> GPU Rendering Mode", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "E            : Toggle 64-bit GPU Emulation", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "J            : Toggle Julia Explorer Mode", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "B            : Toggle Burning Ship Mode", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "N            : Toggle Perturbation Theory Mode", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "S            : Capture Screenshot", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "V            : Start / Stop Video Recording", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "I            : Open / Close Settings Panel", NULL);

        sfons_flush(fons);
    } else {
        sgl_load_pipeline(pip_blend);
        sgl_begin_quads();
        float bg_h = 3.0f * lh + 20.0f;
        if (state->m_tour.phase != TOUR_IDLE) bg_h += lh;
        float bg_w = 700.0f;

        sgl_c4b(20, 20, 25, 220);
        sgl_v2f(5.0f, 5.0f);
        sgl_v2f(bg_w, 5.0f);
        sgl_v2f(bg_w, bg_h);
        sgl_v2f(5.0f, bg_h);
        sgl_v2f(5.0f, 5.0f);
        sgl_end();

        fonsClearState(fons);
        fonsSetFont(fons, font_id);
        fonsSetSize(fons, visual_font_size);
        fonsSetAlign(fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        float x = 15.0f, y = 12.0f;
        char buf[256];

        fonsSetColor(fons, sfons_rgba(255, 255, 255, 255));
        const char* engine_name = "CPU (64-bit)";
        if (gpu_mode) {
#ifdef BUILD_PERTURBATION
            if (active_perturbation_last) {
                engine_name = "GPU (Perturbation)";
            } else if (use_perturbation &&
                       (state->cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD) &&
                       !state->julia_mode && !state->base_fractal) {
                if ((float)state->cam.view.zoom == 0.0f) {
                    engine_name = "GPU (Perturbation)";
                } else {
                    engine_name = "GPU (32-bit)";
                }
            } else {
                engine_name = high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
            }
#else
            engine_name = high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
#endif
        } else {
#ifdef USE_SIMD_F128
            engine_name = cpu_precision_128 ? "CPU (128-bit)" : "CPU (64-bit)";
#endif
        }
        snprintf(buf, sizeof(buf), "[ENGINE] %s | Mode: %s | Threads: %d | Render: %u ms",
                 engine_name,
                 state->julia_mode ? "Julia"
                                     : (state->base_fractal == RENDER_BURNING_SHIP ? "Burning Ship" : (state->base_fractal == RENDER_TRICORN ? "Tricorn" : (state->base_fractal == RENDER_CELTIC ? "Celtic" : (state->base_fractal == RENDER_BUFFALO ? "Buffalo" : "Mandelbrot")))),
                 state->thread_count, state->render_time_ms);
        fonsDrawText(fons, x, y, buf, NULL);
        y += lh;

        if (state->julia_mode) {
            snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", state->julia_c.re,
                     state->julia_c.im);
        } else {
            snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)",
                     (double)state->cam.view.center_re, (double)state->cam.view.center_im);
        }
        fonsDrawText(fons, x, y, buf, NULL);
        y += lh;

        snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
                 (double)state->cam.view.zoom, state->max_iterations,
                 get_palette_name(state->palette_idx % get_palette_count()));
        fonsDrawText(fons, x, y, buf, NULL);
        y += lh;

        if (state->m_tour.phase != TOUR_IDLE) {
            int t_idx = get_tour_target_idx(&state->m_tour);
            int t_tot = get_num_tour_targets(state->base_fractal);
            double t_re = get_tour_target_re(&state->m_tour, state->base_fractal);
            double t_im = get_tour_target_im(&state->m_tour, state->base_fractal);
            snprintf(buf, sizeof(buf), "[TOUR]   Auto-Zoom [%s] Target #%d/%d (%.4f, %.4f)",
                     get_tour_phase_name(state->m_tour.phase), t_idx + 1, t_tot, t_re, t_im);
            fonsDrawText(fons, x, y, buf, NULL);
            y += lh;
        }

        sfons_flush(fons);
    }

    // draw stacked notifications overlay in the bottom right corner
    int draw_count = 0;
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            uint32_t elapsed = now - state->notifications[i].start_time;
            if (elapsed > 3000) {
                state->notifications[i].active = 0; // clear the flag
            } else {
                sgl_load_pipeline(pip_blend);
                sgl_begin_quads();
                float tw = 260.0f;
                float th = 40.0f;
                float trx = ((float)win_w - tw) - 20.0f;
                float try = (float)win_h - th - 20.0f - (float)draw_count * (th + 10.0f);

                sgl_c4b(15, 15, 20, 230); // charcoal background (black with border)
                sgl_v2f(trx, try);
                sgl_v2f(trx + tw, try);
                sgl_v2f(trx + tw, try + th);
                sgl_v2f(trx, try + th);
                sgl_end();

                sgl_begin_lines();
                sgl_c4b(255, 60, 60, 255); // red border
                sgl_v2f(trx, try); sgl_v2f(trx + tw, try);
                sgl_v2f(trx + tw, try); sgl_v2f(trx + tw, try + th);
                sgl_v2f(trx + tw, try + th); sgl_v2f(trx, try + th);
                sgl_v2f(trx, try + th); sgl_v2f(trx, try);
                sgl_end();

                fonsClearState(fons);
                fonsSetFont(fons, font_id);
                fonsSetSize(fons, FONT_SIZE);
                fonsSetAlign(fons, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
                fonsSetColor(fons, sfons_rgba(255, 255, 255, 255));
                fonsDrawText(fons, trx + tw / 2.0f, try + th / 2.0f, state->notifications[i].message, NULL);
                sfons_flush(fons);

                draw_count++;
            }
        }
    }

    // draw mega screenshot progress overlay in the bottom left corner if active
    if (state->mega_screenshot_active == 1) {
        sgl_load_pipeline(pip_blend);
        sgl_begin_quads();
        float tw = 260.0f;
        float th = 40.0f;
        float trx = 20.0f;
        float try = (float)win_h - th - 20.0f;

        sgl_c4b(15, 15, 20, 230); // charcoal background
        sgl_v2f(trx, try);
        sgl_v2f(trx + tw, try);
        sgl_v2f(trx + tw, try + th);
        sgl_v2f(trx, try + th);

        // draw green progress bar at the bottom of the box
        float pb_w = (tw - 10.0f) * (float)state->mega_screenshot_progress / 100.0f;
        sgl_c4b(40, 200, 40, 255); // green progress
        sgl_v2f(trx + 5, try + th - 6);
        sgl_v2f(trx + 5 + pb_w, try + th - 6);
        sgl_v2f(trx + 5 + pb_w, try + th - 3);
        sgl_v2f(trx + 5, try + th - 3);
        sgl_end();

        sgl_begin_lines();
        sgl_c4b(255, 60, 60, 255); // red border
        sgl_v2f(trx, try); sgl_v2f(trx + tw, try);
        sgl_v2f(trx + tw, try); sgl_v2f(trx + tw, try + th);
        sgl_v2f(trx + tw, try + th); sgl_v2f(trx, try + th);
        sgl_v2f(trx, try + th); sgl_v2f(trx, try);
        sgl_end();

        fonsClearState(fons);
        fonsSetFont(fons, font_id);
        fonsSetSize(fons, FONT_SIZE);
        fonsSetAlign(fons, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
        fonsSetColor(fons, sfons_rgba(255, 255, 255, 255));
        char progress_msg[64];
        snprintf(progress_msg, sizeof(progress_msg), "Generating 8K: %d%%", state->mega_screenshot_progress);
        fonsDrawText(fons, trx + tw / 2.0f, try + th / 2.0f - 2.0f, progress_msg, NULL);
        sfons_flush(fons);
    }
}

void hud_render_sokol_deep(FONScontext* fons, int font_id, AppCommonState* state,
                           int win_w, int win_h, int high_precision_mode,
                           int active_perturbation_last, int use_perturbation,
                           sgl_pipeline pip_blend, uint32_t now) {
    float visual_font_size = FONT_SIZE;
    float lh = visual_font_size + 6.0f;

    if (state->show_help) {
        sgl_load_pipeline(pip_blend);
        sgl_begin_quads();
        float w = 580.0f;
        float h = 340.0f;
        float rx = (win_w - w) / 2.0f;
        float ry = (win_h - h) / 2.0f;

        sgl_c4b(15, 15, 20, 245);
        sgl_v2f(rx, ry);
        sgl_v2f(rx + w, ry);
        sgl_v2f(rx + w, ry + h);
        sgl_v2f(rx, ry + h);
        sgl_end();

        sgl_begin_lines();
        sgl_c4b(255, 0, 0, 255);
        sgl_v2f(rx, ry); sgl_v2f(rx + w, ry);
        sgl_v2f(rx + w, ry); sgl_v2f(rx + w, ry + h);
        sgl_v2f(rx + w, ry + h); sgl_v2f(rx, ry + h);
        sgl_v2f(rx, ry + h); sgl_v2f(rx, ry);
        sgl_end();

        fonsClearState(fons);
        fonsSetFont(fons, font_id);
        fonsSetSize(fons, visual_font_size);
        fonsSetAlign(fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        float tx = rx + 25.0f;
        float ty = ry + 20.0f;

        fonsSetColor(fons, sfons_rgba(255, 60, 60, 255));
        fonsDrawText(fons, tx, ty, "[ Keyboard Controls Guide ]", NULL);
        ty += lh * 2.0f;

        fonsSetColor(fons, sfons_rgba(255, 255, 255, 255));
        fonsDrawText(fons, tx, ty, "H / F5       : Toggle Help / Reload Shaders", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "ESC / Q      : Quit Application", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "Ctrl + Z     : Undo Camera Zoom/Pan", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "R            : Reset View", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "P / 0-9      : Cycle / Select Color Palette", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "UP / DOWN    : Adjust Iterations (Shift x10)", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "E            : Toggle 64-bit GPU Emulation (Dekker)", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "N            : Toggle Perturbation Theory Mode", NULL); ty += lh;
        fonsDrawText(fons, tx, ty, "S            : Capture Screenshot", NULL);

        sfons_flush(fons);
    } else {
        sgl_load_pipeline(pip_blend);
        sgl_begin_quads();
        float bg_h = 3.0f * lh + 20.0f;
        float bg_w = 700.0f;

        sgl_c4b(20, 20, 25, 220);
        sgl_v2f(5.0f, 5.0f);
        sgl_v2f(bg_w, 5.0f);
        sgl_v2f(bg_w, bg_h);
        sgl_v2f(5.0f, bg_h);
        sgl_v2f(5.0f, 5.0f);
        sgl_end();

        fonsClearState(fons);
        fonsSetFont(fons, font_id);
        fonsSetSize(fons, visual_font_size);
        fonsSetAlign(fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        float x = 15.0f, y = 12.0f;
        char buf[256];

        const char* engine_name = "GPU (32-bit)";
        if (active_perturbation_last) {
            engine_name = "GPU (Perturbation)";
        } else if (use_perturbation && (state->cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD)) {
            if ((float)state->cam.view.zoom == 0.0f) {
                engine_name = "GPU (Perturbation: No Float!)";
            } else {
                engine_name = "GPU (Perturbation: Fallback)";
            }
        } else {
            engine_name = high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
        }

        snprintf(buf, sizeof(buf), "[ENGINE] %s | Mode: Mandelbrot (Deep Zoom Mode)", engine_name);
        fonsDrawText(fons, x, y, buf, NULL);
        y += lh;

        snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)",
                 (double)state->cam.view.center_re, (double)state->cam.view.center_im);
        fonsDrawText(fons, x, y, buf, NULL);
        y += lh;

        snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
                 (double)state->cam.view.zoom, state->max_iterations,
                 get_palette_name(state->palette_idx % get_palette_count()));
        fonsDrawText(fons, x, y, buf, NULL);
        y += lh;

        sfons_flush(fons);
    }

    // draw stacked notifications overlay in the bottom right corner
    int draw_count = 0;
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            uint32_t elapsed = now - state->notifications[i].start_time;
            if (elapsed > 3000) {
                state->notifications[i].active = 0; // clear the flag
            } else {
                sgl_load_pipeline(pip_blend);
                sgl_begin_quads();
                float tw = 260.0f;
                float th = 40.0f;
                float trx = ((float)win_w - tw) - 20.0f;
                float try = (float)win_h - th - 20.0f - (float)draw_count * (th + 10.0f);

                sgl_c4b(15, 15, 20, 230); // charcoal background (black with border)
                sgl_v2f(trx, try);
                sgl_v2f(trx + tw, try);
                sgl_v2f(trx + tw, try + th);
                sgl_v2f(trx, try + th);
                sgl_end();

                sgl_begin_lines();
                sgl_c4b(255, 60, 60, 255); // red border
                sgl_v2f(trx, try); sgl_v2f(trx + tw, try);
                sgl_v2f(trx + tw, try); sgl_v2f(trx + tw, try + th);
                sgl_v2f(trx + tw, try + th); sgl_v2f(trx, try + th);
                sgl_v2f(trx, try + th); sgl_v2f(trx, try);
                sgl_end();

                fonsClearState(fons);
                fonsSetFont(fons, font_id);
                fonsSetSize(fons, FONT_SIZE);
                fonsSetAlign(fons, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
                fonsSetColor(fons, sfons_rgba(255, 255, 255, 255));
                fonsDrawText(fons, trx + tw / 2.0f, try + th / 2.0f, state->notifications[i].message, NULL);
                sfons_flush(fons);

                draw_count++;
            }
        }
    }
}

void sokol_handle_mouse(AppCommonState* state, const struct sapp_event* event) {
    switch (event->type) {
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            camera_handle_wheel(&state->cam, (double)event->scroll_y, state->cam.mouse_x,
                                state->cam.mouse_y);
            state->needs_redraw = 1;
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN: {
            int btn = (event->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            camera_handle_mouse_down(&state->cam, btn, (int)event->mouse_x, (int)event->mouse_y);
            break;
        }

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            camera_handle_mouse_motion(&state->cam, (int)event->mouse_x, (int)event->mouse_y);
            if (state->cam.is_panning) {
                state->needs_redraw = 1;
            } else if (state->julia_mode && !state->cam.is_zooming) {
                // track mouse for real-time julia
                app_state_get_mouse_coord(state, state->cam.mouse_x, state->cam.mouse_y,
                                          &state->julia_c.re, &state->julia_c.im);
                state->needs_redraw = 1;
            }
            break;

        case SAPP_EVENTTYPE_MOUSE_UP: {
            int btn = (event->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            if (camera_handle_mouse_up(&state->cam, btn)) {
                state->needs_redraw = 1;
            }
            break;
        }

        default:
            break;
    }
}

int sokol_handle_common_keydown(AppCommonState* state, const struct sapp_event* event, uint32_t now) {
    if (event->key_code == SAPP_KEYCODE_ESCAPE || event->key_code == SAPP_KEYCODE_Q) {
        sapp_request_quit();
        return 1;
    } else if (event->key_code == SAPP_KEYCODE_H) {
        state->show_help = !state->show_help;
        return 1;
    } else if (event->key_code == SAPP_KEYCODE_Z && (event->modifiers & SAPP_MODIFIER_CTRL)) {
        if (camera_pop_history(&state->cam)) {
            state->needs_redraw = 1;
            app_state_push_notification(state, "Undo Camera Action", now);
        }
        return 1;
    } else if (event->key_code == SAPP_KEYCODE_R) {
        app_state_reset(state, sapp_set_window_title);
        app_state_push_notification(state, "View Reset!", now);
        return 1;
    } else if (event->key_code == SAPP_KEYCODE_P) {
        app_state_cycle_palette(state);
        char buf[64];
        snprintf(buf, sizeof(buf), "Palette: %s", get_palette_name(state->palette_idx % get_palette_count()));
        app_state_push_notification(state, buf, now);
        return 1;
    } else if (event->key_code >= SAPP_KEYCODE_0 && event->key_code <= SAPP_KEYCODE_9) {
        state->palette_idx = event->key_code - SAPP_KEYCODE_0;
        state->needs_redraw = 1;
        char buf[64];
        snprintf(buf, sizeof(buf), "Palette: %s", get_palette_name(state->palette_idx % get_palette_count()));
        app_state_push_notification(state, buf, now);
        return 1;
    } else if (event->key_code == SAPP_KEYCODE_UP || event->key_code == SAPP_KEYCODE_DOWN) {
        int step = state->max_iterations / 10;
        if (step < 10) step = 10;
        if (event->modifiers & SAPP_MODIFIER_SHIFT) step *= 10;
        state->max_iterations += (event->key_code == SAPP_KEYCODE_UP) ? step : -step;
        if (state->max_iterations < 10) state->max_iterations = 10;
        if (state->max_iterations > get_config_max_iterations_limit())
            state->max_iterations = get_config_max_iterations_limit();
        init_color_palette(state->max_iterations, state->palette_idx);
        state->needs_redraw = 1;
        return 1;
    }
    return 0;
}
