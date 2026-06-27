#ifndef APP_STATE_H
#define APP_STATE_H

#include "camera.h"
#include "core_math.h"
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
    int show_help;
    uint32_t render_time_ms;
    int thread_count;

    // screenshot state
    volatile int mega_screenshot_active;
    volatile int mega_screenshot_progress;

    // notification system (max 5 active stacked notifications)
    struct {
        char message[64];
        uint32_t start_time;
        int active;
    } notifications[5];
} AppCommonState;

void app_state_push_notification(AppCommonState* state, const char* msg, uint32_t now);
void app_state_update_or_push_notification(AppCommonState* state, const char* search_msg, const char* new_msg, uint32_t now);
int app_state_has_active_notifications(const AppCommonState* state);

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
