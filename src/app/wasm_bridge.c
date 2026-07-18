/* wasm_bridge.c
 *
 * webassembly/javascript interop layer for the web build.
 * provides bindings between the c engine and the browser environment (emscripten).
 * handles exporting state to the javascript ui, receiving input events from html elements,
 * and managing local storage for bookmarks.
 */
#include "wasm_bridge.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include "tour.h"
#include "bookmark.h"
#include "color.h"
#include "input_handler.h"
#include "sokol/sokol_time.h"

static AppCommonState* g_state = NULL;

void wasm_bridge_init(AppCommonState* state) {
    g_state = state;
}

EM_JS(void, _call_update_debug_info_js, (int gpu_mode, int julia_mode, int base_fractal, int max_iters, 
                            double zoom, double center_re, double center_im, 
                            int palette_idx, int tour_phase, double julia_re, double julia_im, 
                            int high_precision, int tour_target_idx, int tour_total_targets, 
                            double tour_target_re, double tour_target_im,
                            int thread_count, int render_time_ms), {
    if (typeof window.updateDebugInfo === 'function') {
        window.updateDebugInfo(gpu_mode, julia_mode, base_fractal, max_iters, zoom, center_re, center_im, palette_idx, tour_phase, julia_re, julia_im, high_precision, tour_target_idx, tour_total_targets, tour_target_re, tour_target_im, thread_count, render_time_ms);
    }
})

void call_update_debug_info(int gpu_mode, int julia_mode, int base_fractal, int max_iters, double zoom, double center_re, double center_im, int palette_idx, int tour_phase, double julia_re, double julia_im, int high_precision, int tour_target_idx, int tour_total_targets, double tour_target_re, double tour_target_im, int thread_count, int render_time_ms) {
    _call_update_debug_info_js(gpu_mode, julia_mode, base_fractal, max_iters, zoom, center_re, center_im, palette_idx, tour_phase, julia_re, julia_im, high_precision, tour_target_idx, tour_total_targets, tour_target_re, tour_target_im, thread_count, render_time_ms);
}

/*
 * [WASM EXPORT] wasm_reset_view
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_reset_view(void) {
    if (g_state) {
pthread_mutex_lock(&g_state->state_mutex);
        app_state_reset(g_state, NULL);
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_undo_zoom
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_undo_zoom(void) {
    if (g_state && g_state->cam.history_count > 0) {
pthread_mutex_lock(&g_state->state_mutex);
        g_state->cam.view = g_state->cam.history[--g_state->cam.history_count];
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_next_palette
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_next_palette(void) {
    if (g_state) {
pthread_mutex_lock(&g_state->state_mutex);
        g_state->palette_idx = (g_state->palette_idx + 1) % get_palette_count();
        init_color_palette(g_state->max_iterations, g_state->palette_idx);
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_cycle_fractal
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_cycle_fractal(void) {
    if (g_state && !g_state->julia_mode) {
pthread_mutex_lock(&g_state->state_mutex);
        g_state->base_fractal = (g_state->base_fractal + 1) % 5;
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_set_fractal_mode
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_set_fractal_mode(int mode) {
    if (g_state && !g_state->julia_mode) {
pthread_mutex_lock(&g_state->state_mutex);
        g_state->base_fractal = mode;
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_set_palette
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_set_palette(int p) {
    if (g_state) {
pthread_mutex_lock(&g_state->state_mutex);
        g_state->palette_idx = p % get_palette_count();
        init_color_palette(g_state->max_iterations, g_state->palette_idx);
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_toggle_julia
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia(void) {
    if (g_state) {
pthread_mutex_lock(&g_state->state_mutex);
        app_state_toggle_julia(g_state, NULL);
        app_state_push_notification(g_state, g_state->julia_mode ? "julia mode: active" : "julia mode: inactive", stm_ms(stm_now()));
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_toggle_tour
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_toggle_tour(void) {
    if (g_state) {
pthread_mutex_lock(&g_state->state_mutex);
        app_state_toggle_tour(g_state, stm_ms(stm_now()), NULL);
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_toggle_julia_lock
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia_lock(void) {
    if (g_state) {
        pthread_mutex_lock(&g_state->state_mutex);
        g_state->julia_locked = !g_state->julia_locked;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_set_tour_speed
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_set_tour_speed(double speed) {
    if (g_state) {
        pthread_mutex_lock(&g_state->state_mutex);
        if (speed <= 0.0) speed = 1.0;
        g_state->tour_speed_multiplier = speed;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_mouse_down
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_mouse_down(double x, double y) {
    if (g_state) {
        AppInputEvent ie = {0};
        ie.type = INPUT_MOUSE_DOWN;
        ie.mouse_x = x;
        ie.mouse_y = y;
        ie.mouse_btn = 1;
pthread_mutex_lock(&g_state->state_mutex);
        app_handle_input(g_state, &ie, (uint32_t)stm_ms(stm_now()));
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_mouse_up
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_mouse_up(double x, double y) {
    if (g_state) {
        AppInputEvent ie = {0};
        ie.type = INPUT_MOUSE_UP;
        ie.mouse_x = x;
        ie.mouse_y = y;
        ie.mouse_btn = 1;
pthread_mutex_lock(&g_state->state_mutex);
        app_handle_input(g_state, &ie, (uint32_t)stm_ms(stm_now()));
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_mouse_move
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_mouse_move(double x, double y) {
    if (g_state) {
        AppInputEvent ie = {0};
        ie.type = INPUT_MOUSE_MOVE;
        ie.mouse_x = x;
        ie.mouse_y = y;
pthread_mutex_lock(&g_state->state_mutex);
        app_handle_input(g_state, &ie, (uint32_t)stm_ms(stm_now()));
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_wheel
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_wheel(double dx, double dy) {
    if (g_state) {
        AppInputEvent ie = {0};
        ie.type = INPUT_MOUSE_SCROLL;
        ie.scroll_y = dy;
pthread_mutex_lock(&g_state->state_mutex);
        app_handle_input(g_state, &ie, (uint32_t)stm_ms(stm_now()));
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_get_registered_count
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
int wasm_get_registered_count(void) {
    return 5;
}

/*
 * [WASM EXPORT] wasm_get_registered_name
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
const char* wasm_get_registered_name(int idx) {
    const FractalDefinition* def = get_fractal_by_index(idx);
    return def ? def->name : "unknown";
}

/*
 * [WASM EXPORT] wasm_get_registered_display_name
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
const char* wasm_get_registered_display_name(int idx) {
    const FractalDefinition* def = get_fractal_by_index(idx);
    return def ? def->display_name : "Unknown";
}

/*
 * [WASM EXPORT] wasm_get_registered_mode
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
int wasm_get_registered_mode(int idx) {
    const FractalDefinition* def = get_fractal_by_index(idx);
    return def ? def->mode : 0;
}

/*
 * [WASM EXPORT] wasm_get_palette_count
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
int wasm_get_palette_count(void) {
    return get_palette_count();
}

/*
 * [WASM EXPORT] wasm_get_palette_name
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
const char* wasm_get_palette_name(int idx) {
    return get_palette_name(idx);
}

/*
 * [WASM EXPORT] wasm_adjust_iterations
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_adjust_iterations(int delta) {
    if (g_state) {
        pthread_mutex_lock(&g_state->state_mutex);
        g_state->max_iterations += delta;
        if (g_state->max_iterations < 10) g_state->max_iterations = 10;
        if (g_state->max_iterations > MAX_ITERATIONS_LIMIT) g_state->max_iterations = MAX_ITERATIONS_LIMIT;
        init_color_palette(g_state->max_iterations, g_state->palette_idx);
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_is_julia_locked
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
int wasm_is_julia_locked(void) {
    if (g_state) {
        return g_state->julia_locked;
    }
    return 0;
}

/*
 * [WASM EXPORT] wasm_set_view
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_set_view(double re, double im, double z) {
    if (g_state) {
        pthread_mutex_lock(&g_state->state_mutex);
        g_state->cam.view.center_re = re;
        g_state->cam.view.center_im = im;
        g_state->cam.view.zoom = z;
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}

/*
 * [WASM EXPORT] wasm_set_state
 * exported to javascript to allow the web ui to call this functionality.
 */
EMSCRIPTEN_KEEPALIVE
void wasm_set_state(int is_julia, double jre, double jim, int iters, int pal) {
    if (g_state) {
        pthread_mutex_lock(&g_state->state_mutex);
        g_state->julia_mode = is_julia;
        g_state->julia_c.re = jre;
        g_state->julia_c.im = jim;
        g_state->max_iterations = iters;
        g_state->palette_idx = pal % get_palette_count();
        init_color_palette(g_state->max_iterations, g_state->palette_idx);
        g_state->needs_redraw = 1;
        pthread_mutex_unlock(&g_state->state_mutex);
    }
}
#endif
