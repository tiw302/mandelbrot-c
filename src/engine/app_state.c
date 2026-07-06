/* app_state.c
 *
 * shared application state machine and session controller.
 * manages active modes, notification overlays, and tour updates.
 */

#include "app_state.h"
#include "bookmark.h"
#include "config.h"
#include "config_loader.h"
#include "renderer.h"
#include "color.h"
#include "fractal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// initialize defaults for common application state
void app_state_init(AppCommonState* state, int win_w, int win_h) {
    camera_init(&state->cam, win_w, win_h);
    state->julia_c = (complex_t){-0.7, 0.27};
    state->max_iterations = get_config_default_iterations();
    state->palette_idx = get_config_default_palette();
    state->julia_mode = 0;
    state->base_fractal = RENDER_MANDELBROT;
    state->julia_session.active = 0;
    state->current_bookmark_idx = -1;
    state->running = 1;
    state->needs_redraw = 1;
    state->show_help = 0;
    state->mega_screenshot_active = 0;
    state->mega_screenshot_progress = 0;

    // init video studio
    state->video_fps = 60;
    state->video_duration_sec = 10;
    state->video_res_w = 1920;
    state->video_res_h = 1080;
    state->video_target_re = -0.743643887037158704752191506114774;
    state->video_target_im = 0.131825904205311970493132056385139;
    state->video_target_zoom = 1e-12;
    state->video_is_rendering = 0;

    state->thread_count = 0;
    for (int i = 0; i < 5; i++) {
        state->notifications[i].active = 0;
    }
}

// restore initial explorer mode and reset camera view
void app_state_reset(AppCommonState* state, app_title_callback set_title_cb) {
    state->julia_mode = 0;
    state->julia_session.active = 0;
    state->m_tour.phase = TOUR_IDLE;
    state->max_iterations = get_config_default_iterations();
    state->show_help = 0;
    init_color_palette(state->max_iterations, state->palette_idx);
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
        const FractalDefinition* fd = get_fractal_by_mode(RENDER_JULIA);
        if (set_title_cb && fd) set_title_cb(fd->explorer_title);
    } else {
        if (state->julia_session.active) state->cam.view = state->julia_session.mandelbrot_view;
        state->julia_mode = 0;
        const FractalDefinition* fd = get_fractal_by_mode(state->base_fractal);
        if (set_title_cb && fd) set_title_cb(fd->explorer_title);
    }
    state->cam.history_count = 0;
    state->needs_redraw = 1;
}

// toggle burning ship mode and reset camera to default
void app_state_cycle_fractal(AppCommonState* state, app_title_callback set_title_cb) {

    if (state->base_fractal == RENDER_MANDELBROT) state->base_fractal = RENDER_BURNING_SHIP;
    else if (state->base_fractal == RENDER_BURNING_SHIP) state->base_fractal = RENDER_TRICORN;
    else if (state->base_fractal == RENDER_TRICORN) state->base_fractal = RENDER_CELTIC;
    else if (state->base_fractal == RENDER_CELTIC) state->base_fractal = RENDER_BUFFALO;
    else state->base_fractal = RENDER_MANDELBROT;

    state->julia_mode = 0;
    state->julia_session.active = 0;
    state->cam.history_count = 0;

    // each fractal has its own natural default view
    const FractalDefinition* fd = get_fractal_by_mode(state->base_fractal);
    if (fd) {
        state->cam.view.center_re = fd->default_center_re;
        state->cam.view.center_im = fd->default_center_im;
        state->cam.view.zoom = fd->default_zoom;
        if (set_title_cb) {
            set_title_cb(fd->explorer_title);
        }
    }
    state->needs_redraw = 1;
}

// cycle through available palettes and re-initialize color registry
void app_state_cycle_palette(AppCommonState* state) {
    state->palette_idx = (state->palette_idx + 1) % get_palette_count();
    // only rebuild the lut — don't re-initialize the full renderer (that would re-spawn threads)
    init_color_palette(state->max_iterations, state->palette_idx);
    state->needs_redraw = 1;
}

void app_state_save_bookmark_with_name(AppCommonState* state, const char* name) {
    Bookmark b;
    memset(&b, 0, sizeof(b));
    if (name) {
        strncpy(b.name, name, sizeof(b.name) - 1);
    }
    b.center_re = state->cam.view.center_re;
    b.center_im = state->cam.view.center_im;
    b.zoom = state->cam.view.zoom;
    b.max_iterations = state->max_iterations;

    /* if in julia mode, we save it as a julia fractal but also save the julia_c constant.
     * store julia first so julia-mode bookmarks can be distinguished from base fractals. */
    if (state->julia_mode) {
        b.fractal_type = RENDER_JULIA;
        b.julia_c = state->julia_c;
    } else {
        b.fractal_type = state->base_fractal;
    }
    save_bookmark(&b);
}

void app_state_save_bookmark(AppCommonState* state) {
    app_state_save_bookmark_with_name(state, NULL);
}

int app_state_get_bookmark_count(void) {
    return get_bookmark_count();
}

const Bookmark* app_state_get_bookmarks_array(int* out_count) {
    return get_bookmarks_array(out_count);
}

void app_state_delete_bookmark(AppCommonState* state, int index) {
    delete_bookmark(index);
    if (state->current_bookmark_idx == index) {
        state->current_bookmark_idx = -1;
    } else if (state->current_bookmark_idx > index) {
        state->current_bookmark_idx--;
    }
}

// push current view to undo stack and load next saved bookmark
void app_state_load_bookmark(AppCommonState* state, int index) {
    Bookmark b;
    if (load_bookmark(index, &b)) {
        camera_push_history(&state->cam);
        state->current_bookmark_idx = index;
        state->cam.view = (ViewState){b.center_re, b.center_im, b.zoom};
        state->max_iterations = b.max_iterations;
        state->julia_mode = (b.fractal_type == RENDER_JULIA);
        // restore the base fractal; if it was julia mode the base doesn't matter
        state->base_fractal = state->julia_mode ? RENDER_MANDELBROT : b.fractal_type;
        state->julia_c = b.julia_c;
        init_color_palette(state->max_iterations, state->palette_idx);
        state->needs_redraw = 1;
    }
}

// push current view to undo stack and load next saved bookmark
void app_state_load_next_bookmark(AppCommonState* state) {
    int count = get_bookmark_count();
    if (count > 0) {
        int next_idx = (state->current_bookmark_idx + 1) % count;
        app_state_load_bookmark(state, next_idx);
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
            start_tour(&state->m_tour, &state->cam.view, state->base_fractal);
            if (set_title_cb) {
                set_title_cb(state->base_fractal == RENDER_BURNING_SHIP ? "Burning Ship Explorer  [Auto-Zoom]" : (state->base_fractal == RENDER_TRICORN ? "Tricorn Explorer  [Auto-Zoom]" : (state->base_fractal == RENDER_CELTIC ? "Celtic Explorer  [Auto-Zoom]" : (state->base_fractal == RENDER_BUFFALO ? "Buffalo Explorer  [Auto-Zoom]" : "Mandelbrot Explorer  [Auto-Zoom]"))));
            }
        } else {
            stop_tour(&state->m_tour);
            camera_reset(&state->cam);
            if (set_title_cb) {
                set_title_cb(state->base_fractal == RENDER_BURNING_SHIP ? "Burning Ship Explorer" : (state->base_fractal == RENDER_TRICORN ? "Tricorn Explorer" : (state->base_fractal == RENDER_CELTIC ? "Celtic Explorer" : (state->base_fractal == RENDER_BUFFALO ? "Buffalo Explorer" : "Mandelbrot Explorer"))));
            }
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
            update_tour(&state->m_tour, &state->cam.view, now, state->base_fractal);
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

void app_state_push_notification(AppCommonState* state, const char* msg, uint32_t now) {
    // shift active notifications up (from bottom/index 0 upwards)
    for (int i = 4; i > 0; i--) {
        state->notifications[i] = state->notifications[i - 1];
    }
    // put the newest notification at index 0 (bottom of the stack)
    snprintf(state->notifications[0].message, sizeof(state->notifications[0].message), "%s", msg);
    state->notifications[0].start_time = now;
    state->notifications[0].active = 1;
}

void app_state_update_or_push_notification(AppCommonState* state, const char* search_msg, const char* new_msg, uint32_t now) {
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active && strstr(state->notifications[i].message, search_msg) != NULL) {
            snprintf(state->notifications[i].message, sizeof(state->notifications[i].message), "%s", new_msg);
            state->notifications[i].start_time = now;
            return;
        }
    }
    app_state_push_notification(state, new_msg, now);
}

int app_state_has_active_notifications(const AppCommonState* state) {
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            return 1;
        }
    }
    return 0;
}
