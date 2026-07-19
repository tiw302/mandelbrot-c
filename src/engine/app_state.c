/* app_state.c
 *
 * shared application state machine and session controller.
 * manages active modes, notification overlays, and tour updates.
 */

#include "app_state.h"

#include "bookmark.h"
#include "color.h"
#include "config.h"
#include "config_loader.h"
#include "fractal.h"
#include "renderer.h"
#ifdef USE_SIMD_F128
#include "simd_f128_io.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAFE_STRCPY(dest, src)                       \
    do {                                             \
        snprintf((dest), sizeof(dest), "%s", (src)); \
    } while (0)

CLIArgs g_cli_args = {0};

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
    state->tour_speed_multiplier = 1.0;
    state->zoom_sensitivity = 1.0;

    // init video studio
    state->video_settings.fps = 60;
    state->video_settings.duration_sec = 10;
    state->video_settings.res_w = 1280;
    state->video_settings.res_h = 720;
    state->video_settings.preset_idx = 4;  // fast
    state->video_settings.crf = 1;         // high (CRF 18)
    state->video_settings.aa_level = 0;    // none (1x)
    state->video_settings.codec_idx = 0;   // H.264
    state->video_settings.show_log = 0;    // hide by default
    SAFE_STRCPY(state->video_settings.target_re, "-0.743643887037158704752191506114774");
    SAFE_STRCPY(state->video_settings.target_im, "0.131825904205311970493132056385139");
    SAFE_STRCPY(state->video_settings.target_zoom, "1e-12");
    state->video_settings.is_rendering = 0;
    state->video_settings.path_type = 0;  // default to Scenic Tour
    state->video_settings.log_fontsize = 20;
    app_state_resolve_asset_path("assets/fonts/font.ttf", state->video_settings.log_fontpath,
                                 sizeof(state->video_settings.log_fontpath));
    state->video_settings.crf_val = 18;
    state->video_settings.zoom_curve = 0;
    state->video_settings.log_position = 0;
    state->video_settings.log_opacity = 0.6f;
    SAFE_STRCPY(state->video_settings.log_fontcolor, "white");
    SAFE_STRCPY(state->video_settings.output_filename, "mandelbrot_video.mp4");

    if (g_cli_args.parsed) {
        if (g_cli_args.width > 0) state->video_settings.res_w = g_cli_args.width;
        if (g_cli_args.height > 0) state->video_settings.res_h = g_cli_args.height;
        if (g_cli_args.fps > 0) state->video_settings.fps = g_cli_args.fps;
        if (g_cli_args.duration > 0) state->video_settings.duration_sec = g_cli_args.duration;
        if (g_cli_args.out[0] != '\0')
            SAFE_STRCPY(state->video_settings.output_filename, g_cli_args.out);
        if (g_cli_args.crf >= 0) state->video_settings.crf_val = g_cli_args.crf;
        if (g_cli_args.curve >= 0) state->video_settings.zoom_curve = g_cli_args.curve;
        if (g_cli_args.log >= 0) state->video_settings.show_log = g_cli_args.log;
        if (g_cli_args.log_size > 0) state->video_settings.log_fontsize = g_cli_args.log_size;
        if (g_cli_args.log_font[0] != '\0')
            SAFE_STRCPY(state->video_settings.log_fontpath, g_cli_args.log_font);
        if (g_cli_args.log_pos >= 0) state->video_settings.log_position = g_cli_args.log_pos;
        if (g_cli_args.log_opacity >= 0.0f)
            state->video_settings.log_opacity = g_cli_args.log_opacity;
        if (g_cli_args.log_color[0] != '\0')
            SAFE_STRCPY(state->video_settings.log_fontcolor, g_cli_args.log_color);

        if (strcmp(g_cli_args.path, "scenic") == 0)
            state->video_settings.path_type = 0;
        else if (strcmp(g_cli_args.path, "bookmarks") == 0)
            state->video_settings.path_type = 1;
        else if (strcmp(g_cli_args.path, "custom") == 0)
            state->video_settings.path_type = 2;

        if (g_cli_args.preset[0] != '\0') {
            const char* presets[] = {"ultrafast", "superfast", "veryfast", "faster",  "fast",
                                     "medium",    "slow",      "slower",   "veryslow"};
            for (int i = 0; i < 9; i++) {
                if (strcmp(g_cli_args.preset, presets[i]) == 0) {
                    state->video_settings.preset_idx = i;
                    break;
                }
            }
        }

        if (strcmp(g_cli_args.codec, "h264") == 0)
            state->video_settings.codec_idx = 0;
        else if (strcmp(g_cli_args.codec, "h265") == 0)
            state->video_settings.codec_idx = 1;

        if (g_cli_args.aa == 1)
            state->video_settings.aa_level = 0;
        else if (g_cli_args.aa == 2)
            state->video_settings.aa_level = 1;
        else if (g_cli_args.aa == 4)
            state->video_settings.aa_level = 2;
    }

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
    if (state->base_fractal == RENDER_MANDELBROT)
        state->base_fractal = RENDER_BURNING_SHIP;
    else if (state->base_fractal == RENDER_BURNING_SHIP)
        state->base_fractal = RENDER_TRICORN;
    else if (state->base_fractal == RENDER_TRICORN)
        state->base_fractal = RENDER_CELTIC;
    else if (state->base_fractal == RENDER_CELTIC)
        state->base_fractal = RENDER_BUFFALO;
    else
        state->base_fractal = RENDER_MANDELBROT;

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

// cycle forward through saved bookmarks
void app_state_load_next_bookmark(AppCommonState* state) {
    int count = get_bookmark_count();
    if (count > 0) {
        int next_idx = (state->current_bookmark_idx + 1) % count;
        app_state_load_bookmark(state, next_idx);
    }
}

/* map base fractal mode to window title.
 * touring=1 appends "[Auto-Zoom]" for the tour state. */
static const char* fractal_display_name(int base_fractal, int touring) {
    /* use static buffers — only one title is active at a time */
    static char tour_title[128];
    static char idle_title[128];
    const FractalDefinition* fd = get_fractal_by_mode(base_fractal);
    const char* base = fd ? fd->explorer_title : "Mandelbrot Explorer";
    if (touring) {
        snprintf(tour_title, sizeof(tour_title), "%s  [Auto-Zoom]", base);
        return tour_title;
    }
    snprintf(idle_title, sizeof(idle_title), "%s", base);
    return idle_title;
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
            if (set_title_cb) set_title_cb(fractal_display_name(state->base_fractal, 1));
        } else {
            stop_tour(&state->m_tour);
            camera_reset(&state->cam);
            if (set_title_cb) set_title_cb(fractal_display_name(state->base_fractal, 0));
            state->needs_redraw = 1;
        }
    }
}

// tick active animation state machine based on elapsed time
void app_state_update_tours(AppCommonState* state, uint32_t now) {
    if (state->julia_mode) {
        if (state->j_tour.phase != JULIA_TOUR_IDLE) {
            update_julia_tour(&state->j_tour, &state->julia_c, now, state->tour_speed_multiplier);
            state->needs_redraw = 1;
        }
    } else {
        if (state->m_tour.phase != TOUR_IDLE) {
            update_tour(&state->m_tour, &state->cam.view, now, state->base_fractal,
                        state->tour_speed_multiplier);
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

precise_float parse_precise_float(const char* str) {
#ifdef USE_SIMD_F128
    simd_f128 val = simd_f128_from_string(str);
    double hi, lo;
    simd_f128_extract(val, &hi, &lo);
    return (precise_float)hi + (precise_float)lo;
#else
    return (precise_float)strtold(str, NULL);
#endif
}

void app_state_start_video_render(AppCommonState* state, uint32_t now) {
    double total_ms = state->video_settings.duration_sec * 1000.0;

    if (state->video_settings.path_type == 0 || state->video_settings.path_type == 1) {
        start_tour(&state->m_tour, &state->cam.view, state->base_fractal);
        state->m_tour.is_dynamic = (state->video_settings.path_type == 1);
        state->m_tour.zoom_curve = state->video_settings.zoom_curve;

        state->m_tour.pan_ms = total_ms * 0.20;
        state->m_tour.zoom_in_ms = total_ms * 0.45;
        state->m_tour.zoom_out_ms = total_ms * 0.35;
    } else {
        state->m_tour.home_re = state->cam.view.center_re;
        state->m_tour.home_im = state->cam.view.center_im;
        state->m_tour.home_zoom = state->cam.view.zoom;
        state->m_tour.target_re = parse_precise_float(state->video_settings.target_re);
        state->m_tour.target_im = parse_precise_float(state->video_settings.target_im);
        state->m_tour.deep_zoom = parse_precise_float(state->video_settings.target_zoom);
        state->m_tour.zoom_curve = state->video_settings.zoom_curve;

        state->m_tour.pan_ms = total_ms * 0.30;
        state->m_tour.zoom_in_ms = total_ms * 0.70;
        state->m_tour.zoom_out_ms = 0.0;
        state->m_tour.is_dynamic = 0;
        state->m_tour.phase = TOUR_PANNING;
        state->m_tour.phase_start = now;
    }

    if (state->julia_mode) {
        start_julia_tour(&state->j_tour, &state->julia_c, now);
        double scale = (double)state->video_settings.duration_sec / 10.0;
        if (scale < 0.1) scale = 0.1;
        state->j_tour.move_ms = JULIA_TOUR_MOVE_MS * scale;
        state->j_tour.dwell_ms = JULIA_TOUR_DWELL_MS * scale;
    }

    state->video_settings.is_rendering = 1;
}

void app_state_step_simulation(AppCommonState* state, uint32_t now) {
    app_state_update_tours(state, now);
}

void app_state_resolve_asset_path(const char* relative_path, char* out_path, size_t max_len) {
    const char* prefixes[] = {"./", "../", "../../", "../../../", ""};
    char temp[512];
    for (int i = 0; i < 5; i++) {
        snprintf(temp, sizeof(temp), "%s%s", prefixes[i], relative_path);
        FILE* f = fopen(temp, "r");
        if (f) {
            fclose(f);
            /* resolve to absolute path so downstream tools (e.g. ffmpeg drawtext)
             * can locate the file regardless of working directory. */
            char abs[512];
            if (realpath(temp, abs) != NULL) {
                strncpy(out_path, abs, max_len - 1);
            } else {
                strncpy(out_path, temp, max_len - 1);
            }
            out_path[max_len - 1] = '\0';
            return;
        }
    }
    // fallback if not found — try realpath on the raw relative path as-is
    char abs[512];
    if (realpath(relative_path, abs) != NULL) {
        strncpy(out_path, abs, max_len - 1);
    } else {
        strncpy(out_path, relative_path, max_len - 1);
    }
    out_path[max_len - 1] = '\0';
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
