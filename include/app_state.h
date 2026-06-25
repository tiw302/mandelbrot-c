#ifndef APP_STATE_H
#define APP_STATE_H

#include "bookmark.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "ini_config.h"
#include "julia.h"
#include "mandelbrot.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"

// transient state for julia mode transitions
typedef struct {
    ViewState mandelbrot_view;
    int active;
} JuliaSession;

// callback to update window title across different frontend backends
typedef void (*app_title_callback)(const char* title);

// unified application common state
typedef struct {
    // navigation state
    Camera cam;

    // runtime modes
    TourState m_tour;
    JuliaTourState j_tour;
    int julia_mode;
    int burning_ship_mode;
    complex_t julia_c;
    JuliaSession julia_session;

    // renderer parameters
    int max_iterations;
    int palette_idx;
    int current_bookmark_idx;
    int needs_redraw;
    int running;
    uint32_t render_time_ms;
} AppCommonState;

// initializes state
void app_state_init(AppCommonState* state, int win_w, int win_h);

// resets view and iterations to default
void app_state_reset(AppCommonState* state, app_title_callback set_title_cb);

// toggles julia explorer mode
void app_state_toggle_julia(AppCommonState* state, app_title_callback set_title_cb);

// toggles burning ship fractal mode
void app_state_toggle_burning_ship(AppCommonState* state, app_title_callback set_title_cb);

// cycles to next color palette
void app_state_cycle_palette(AppCommonState* state);

// saves active view as bookmark
void app_state_save_bookmark(AppCommonState* state);

// cycles through and loads bookmarks
void app_state_load_next_bookmark(AppCommonState* state);

// toggles tour on/off
void app_state_toggle_tour(AppCommonState* state, uint32_t now, app_title_callback set_title_cb);

// updates active tours (auto-zoom or auto-c shift)
void app_state_update_tours(AppCommonState* state, uint32_t now, app_title_callback set_title_cb);

// maps mouse coordinates to complex plane
void app_state_get_mouse_coord(const AppCommonState* state, int mx, int my, double* re, double* im);

// computes complex plane coordinates for viewport boundaries
void app_state_calculate_boundaries(const AppCommonState* state, int width, int height,
                                    precise_float* re_min, precise_float* re_max,
                                    precise_float* im_min, precise_float* im_max);

#endif  // app_state_h
