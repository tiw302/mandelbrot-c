/* app_state.h
 *
 * central application state structures and api.
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <pthread.h>

#include "bookmark.h"
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

// video studio settings and state
typedef struct {
    int fps;
    int duration_sec;
    int res_w;
    int res_h;
    int preset_idx;  // 0=ultrafast to 8=veryslow
    int crf;         // 0=Lossless, 1=High(18), 2=Medium(23), 3=Low(28)
    int aa_level;    // 1, 2, 4
    int codec_idx;   // 0=H.264, 1=H.265
    int show_log;    // 1=show, 0=hide
    char target_re[64];
    char target_im[64];
    char target_zoom[64];
    int is_rendering;
    int path_type;  // 0=Scenic Tour, 1=Bookmarks Tour, 2=Custom Target
    int log_fontsize;
    char log_fontpath[256];
    int crf_val;                // 0-51
    int zoom_curve;             // 0=Ease-In-Out, 1=Linear, 2=Ease-In, 3=Ease-Out
    int log_position;           // 0=Top-Left, 1=Top-Right, 2=Bottom-Left, 3=Bottom-Right
    float log_opacity;          // 0.0 to 1.0
    char log_fontcolor[32];     // "white", "yellow", "cyan", "green", etc.
    char output_filename[256];  // output file path

    float color_cycle_speed;
    char audio_track_path[256];
    int pixel_format_idx;
    int bitrate_mode;
    int target_bitrate_kbps;
    int motion_blur_samples;

    // background video export state
    volatile float export_progress_percent;
    volatile int export_cancelled;
} VideoStudioSettings;

// unified application common state
typedef struct {
    // navigation state
    Camera cam;

    // runtime modes
    TourState m_tour;
    JuliaTourState j_tour;
    int julia_mode;
    int julia_locked;
    int base_fractal;
    complex_t julia_c;
    JuliaSession julia_session;

    // renderer parameters
    int max_iterations;
    int palette_idx;
    int current_bookmark_idx;
    int needs_redraw;
    int running;
    int show_help;
    int show_settings;
    uint32_t render_time_ms;
    int thread_count;
    pthread_mutex_t state_mutex;
    double tour_speed_multiplier;
    double zoom_sensitivity;

    // screenshot state
    volatile int mega_screenshot_active;
    volatile int mega_screenshot_progress;
    char mega_screenshot_filename[256];

    // video studio state
    VideoStudioSettings video_settings;

    // advanced renderer & visual settings
    double bailout_radius;
    int render_tile_size;
    int resolution_scale;
    double color_offset;
    double color_density;

    // notification system (max 5 active stacked notifications)
    struct {
        char message[64];
        uint32_t start_time;
        int active;
    } notifications[5];
} AppCommonState;

void app_state_push_notification(AppCommonState* state, const char* msg, uint32_t now);

void app_state_init(AppCommonState* state, int win_w, int win_h);

// resets view and iterations to default
void app_state_reset(AppCommonState* state, app_title_callback set_title_cb);

// toggles julia explorer mode
void app_state_toggle_julia(AppCommonState* state, app_title_callback set_title_cb);

// cycles through available fractal types
void app_state_cycle_fractal(AppCommonState* state, app_title_callback set_title_cb);

// cycles to next color palette
void app_state_cycle_palette(AppCommonState* state);

// saves active view as bookmark with a custom name (if name is NULL, uses default)
void app_state_save_bookmark_with_name(AppCommonState* state, const char* name);

// saves active view as bookmark (legacy)
void app_state_save_bookmark(AppCommonState* state);

// gets the total number of bookmarks
int app_state_get_bookmark_count(void);

// returns the array of bookmarks
const Bookmark* app_state_get_bookmarks_array(int* out_count);

// deletes a specific bookmark by index
void app_state_delete_bookmark(AppCommonState* state, int index);

// loads a specific bookmark by index
void app_state_load_bookmark(AppCommonState* state, int index);

// cycles through and loads bookmarks
void app_state_load_next_bookmark(AppCommonState* state);

// toggles tour on/off
void app_state_toggle_tour(AppCommonState* state, uint32_t now, app_title_callback set_title_cb);

// updates active tours (auto-zoom or auto-c shift)
void app_state_update_tours(AppCommonState* state, uint32_t now);

// maps mouse coordinates to complex plane
void app_state_get_mouse_coord(const AppCommonState* state, int mx, int my, double* re, double* im);

// computes complex plane coordinates for viewport boundaries
void app_state_calculate_boundaries(const AppCommonState* state, int width, int height,
                                    precise_float* re_min, precise_float* re_max,
                                    precise_float* im_min, precise_float* im_max);

// video studio controller: sets up the tour and starts the video rendering process
void app_state_start_video_render(AppCommonState* state, uint32_t now);

// advances tours and boundaries by one simulation tick
void app_state_step_simulation(AppCommonState* state, uint32_t now);

// resolves a relative asset path (e.g. "assets/fonts/font.ttf") by hunting in common directories
void app_state_resolve_asset_path(const char* relative_path, char* out_path, size_t max_len);

// parses a string into a precise_float
precise_float parse_precise_float(const char* str);

// cli arguments structure
typedef struct {
    int parsed;
    int headless;
    int width;
    int height;
    int fps;
    int duration;
    char path[32];
    char out[256];
    int crf;
    char preset[32];
    char codec[32];
    int aa;
    int log;
    int log_size;
    char log_font[256];
    int log_pos;
    float log_opacity;
    char log_color[32];
    int curve;
    int gpu;
} CLIArgs;

extern CLIArgs g_cli_args;

#endif  // APP_STATE_H
