/* app_state.c
 *
 * manages the global application state lifecycle, event dispatch, and UI coordinate mapping.
 * handles coordinate conversions and state transitions between render modes.
 */

#include "app_state.h"

#include <stdio.h>
#include <stdlib.h>

// initialize defaults for common application state
void app_state_init(AppCommonState* state, int win_w, int win_h) {
    camera_init(&state->cam, win_w, win_h);
    state->julia_c = (complex_t){-0.7, 0.27};
    state->max_iterations = get_config_default_iterations();
    state->palette_idx = get_config_default_palette();
    state->julia_mode = 0;
    state->burning_ship_mode = 0;
    state->julia_session.active = 0;
    state->current_bookmark_idx = -1;
    state->running = 1;
    state->needs_redraw = 1;
}

// restore initial explorer mode and reset camera view
void app_state_reset(AppCommonState* state, app_title_callback set_title_cb) {
    state->julia_mode = 0;
    state->julia_session.active = 0;
    state->m_tour.phase = TOUR_IDLE;
    state->max_iterations = get_config_default_iterations();
    init_renderer(state->max_iterations, state->palette_idx);
    camera_reset(&state->cam);
    if (set_title_cb) set_title_cb("Mandelbrot Explorer");
    state->needs_redraw = 1;
}

#define JULIA_ZOOM 4.0

// toggle between julia and mandelbrot mode, saving previous mandelbrot viewport
void app_state_toggle_julia(AppCommonState* state, app_title_callback set_title_cb) {
    if (!state->julia_mode) {
        state->julia_session.mandelbrot_view = state->cam.view;
        state->julia_session.active = 1;
        app_state_get_mouse_coord(state, state->cam.mouse_x, state->cam.mouse_y, &state->julia_c.re,
                                  &state->julia_c.im);
        state->cam.view = (ViewState){0.0, 0.0, JULIA_ZOOM};
        state->julia_mode = 1;
        if (set_title_cb) set_title_cb("Julia Explorer");
    } else {
        if (state->julia_session.active) state->cam.view = state->julia_session.mandelbrot_view;
        state->julia_mode = 0;
        if (set_title_cb) set_title_cb("Mandelbrot Explorer");
    }
    state->cam.history_count = 0;
    state->needs_redraw = 1;
}

// toggle burning ship mode and reset camera to default
void app_state_toggle_burning_ship(AppCommonState* state, app_title_callback set_title_cb) {
    state->burning_ship_mode = !state->burning_ship_mode;
    state->julia_mode = 0;
    state->julia_session.active = 0;
    camera_reset(&state->cam);
    if (set_title_cb) {
        set_title_cb(state->burning_ship_mode ? "Burning Ship Explorer" : "Mandelbrot Explorer");
    }
    state->needs_redraw = 1;
}

// cycle through available palettes and re-initialize color registry
void app_state_cycle_palette(AppCommonState* state) {
    state->palette_idx = (state->palette_idx + 1) % get_palette_count();
    init_renderer(state->max_iterations, state->palette_idx);
    state->needs_redraw = 1;
}

// pack active view/state parameters and persist to storage
void app_state_save_bookmark(AppCommonState* state) {
    Bookmark b = {state->cam.view.center_re,
                  state->cam.view.center_im,
                  state->cam.view.zoom,
                  state->max_iterations,
                  state->julia_mode ? 1 : (state->burning_ship_mode ? 2 : 0),
                  state->julia_c};
    save_bookmark(&b);
}

// push current view to undo stack and load next saved bookmark
void app_state_load_next_bookmark(AppCommonState* state) {
    Bookmark b;
    int count = get_bookmark_count();
    if (count > 0) {
        camera_push_history(&state->cam);
        state->current_bookmark_idx = (state->current_bookmark_idx + 1) % count;
        if (load_bookmark(state->current_bookmark_idx, &b)) {
            state->cam.view = (ViewState){b.center_re, b.center_im, b.zoom};
            state->max_iterations = b.max_iterations;
            state->julia_mode = (b.fractal_type == 1);
            state->burning_ship_mode = (b.fractal_type == 2);
            state->julia_c = b.julia_c;
            init_renderer(state->max_iterations, state->palette_idx);
            state->needs_redraw = 1;
        }
    }
}

// toggle auto-navigation tour on/off depending on active mode
void app_state_toggle_tour(AppCommonState* state, uint32_t now, app_title_callback set_title_cb) {
    if (state->julia_mode) {
        if (state->j_tour.phase == JULIA_TOUR_IDLE) {
            start_julia_tour(&state->j_tour, &state->julia_c, now);
            if (set_title_cb) set_title_cb("Julia Explorer  [Auto-c]");
        } else {
            stop_julia_tour(&state->j_tour);
            if (set_title_cb) set_title_cb("Julia Explorer");
            state->needs_redraw = 1;
        }
    } else {
        if (state->m_tour.phase == TOUR_IDLE) {
            state->julia_mode = state->julia_session.active = 0;
            camera_reset(&state->cam);
            start_tour(&state->m_tour, &state->cam.view);
            if (set_title_cb) set_title_cb("Mandelbrot Explorer  [Auto-Zoom]");
        } else {
            stop_tour(&state->m_tour);
            camera_reset(&state->cam);
            if (set_title_cb) set_title_cb("Mandelbrot Explorer");
            state->needs_redraw = 1;
        }
    }
}

// tick active animation state machine based on elapsed time
void app_state_update_tours(AppCommonState* state, uint32_t now, app_title_callback set_title_cb) {
    if (state->julia_mode) {
        if (state->j_tour.phase != JULIA_TOUR_IDLE) {
            update_julia_tour(&state->j_tour, &state->julia_c, now);
            state->needs_redraw = 1;
        }
    } else {
        if (state->m_tour.phase != TOUR_IDLE) {
            update_tour(&state->m_tour, &state->cam.view, now, state->burning_ship_mode);
            state->needs_redraw = 1;
        }
    }
}

// map viewport screen coordinates to complex plane values
void app_state_get_mouse_coord(const AppCommonState* state, int mx, int my, double* re,
                               double* im) {
    precise_float pre, pim;
    camera_screen_to_complex(&state->cam, mx, my, &pre, &pim);
    *re = (double)pre;
    *im = (double)pim;
}

/* compute complex coordinate bounds for the current camera viewport,
   taking screen aspect ratio correction into account */
void app_state_calculate_boundaries(const AppCommonState* state, int width, int height,
                                    precise_float* re_min, precise_float* re_max,
                                    precise_float* im_min, precise_float* im_max) {
    precise_float aspect = (precise_float)width / height;
    *re_min = state->cam.view.center_re - state->cam.view.zoom * aspect / 2.0;
    *re_max = state->cam.view.center_re + state->cam.view.zoom * aspect / 2.0;
    *im_max = state->cam.view.center_im + state->cam.view.zoom / 2.0;
    *im_min = state->cam.view.center_im - state->cam.view.zoom / 2.0;
}
