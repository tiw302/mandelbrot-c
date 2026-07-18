/* screenshot.c
 *
 * captures viewport frames and exports png files.
 * handles asynchronous rendering and saving of mega 8k screenshots.
 *
 * features:
 *   - creates standard viewport frames and writes to PNG using stb_image_write
 *   - schedules mega screenshot (e.g. 7680x4320) split into horizontal or vertical bands
 *   - runs high-res render loops incrementally to keep the UI thread responsive
 */

#include "screenshot.h"
/* screenshot.c is the sole owner of stb_image_write implementation. * do not define
 * STB_IMAGE_WRITE_IMPLEMENTATION anywhere else. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if !defined(_WIN32)
#include <signal.h>
#include <unistd.h>
#endif

#include "renderer.h"

int save_mega_screenshot(RendererContext* render_ctx, AppCommonState* state, int target_width,
                         int target_height, precise_float re_min, precise_float re_max,
                         precise_float im_min, precise_float im_max, int max_iterations,
                         int fractal_type, complex_t julia_c) {
    static uint32_t mega_counter = 0;
    char filename[256];
    snprintf(filename, sizeof(filename), "mega_screenshot_%u_%u.tga", (unsigned int)time(NULL),
             ++mega_counter);

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "error: failed to open mega screenshot file for writing\n");
        return 0;
    }

    if (state) {
        pthread_mutex_lock(&state->state_mutex);
        snprintf(state->mega_screenshot_filename, sizeof(state->mega_screenshot_filename), "%s",
                 filename);
        pthread_mutex_unlock(&state->state_mutex);
    }

    /* write tga header (uncompressed true-color image).
     * we choose tga because we can stream it to disk line-by-line or band-by-band
     * without needing to keep the entire huge image (e.g. 7680x4320 at 4 bytes = 132mb)
     * in ram. png requires compressing the entire buffer at once, which is too memory-heavy. */
    uint8_t header[18] = {0};
    header[2] = 2;  // uncompressed rgb
    header[12] = (target_width & 0xFF);
    header[13] = (target_width >> 8);
    header[14] = (target_height & 0xFF);
    header[15] = (target_height >> 8);
    header[16] = 32;    // 32 bits per pixel
    header[17] = 0x20;  // top-down origin
    if (fwrite(header, 1, 18, file) != 18) {
        fprintf(stderr, "error: failed to write tga header\n");
        fclose(file);
        return 0;
    }

    /* we render in chunks (strips) to save memory.
     * by allocating a small 256-line buffer instead of the whole target_height,
     * we bound memory consumption to under 8mb even for massive resolution targets. */
    int chunk_height = 256;
    if (chunk_height > target_height) chunk_height = target_height;

    uint32_t* chunk_pixels =
        (uint32_t*)malloc((size_t)target_width * chunk_height * sizeof(uint32_t));
    if (!chunk_pixels) {
        fprintf(stderr, "error: failed to allocate memory for mega screenshot chunk\n");
        fclose(file);
        return 0;
    }

    precise_float im_step = (im_max - im_min) / target_height;
    int pitch = target_width * 4;

    // renderer palette is initialized by the main thread.

    printf("saving mega screenshot (%dx%d) to %s...\n", target_width, target_height, filename);

    for (int y_start = 0; y_start < target_height; y_start += chunk_height) {
        int lines = chunk_height;
        if (y_start + lines > target_height) {
            lines = target_height - y_start;
        }

        precise_float strip_im_max = im_max - y_start * im_step;
        precise_float strip_im_min = strip_im_max - lines * im_step;

        RenderJob job = {.pixels = chunk_pixels,
                         .pitch = pitch,
                         .window_width = target_width,
                         .window_height = lines,
                         .re_min = re_min,
                         .re_max = re_max,
                         .im_top = strip_im_max,
                         .im_bottom = strip_im_min,
                         .mode = (RenderMode)fractal_type,
                         .julia_c = julia_c,
                         .max_iterations = max_iterations};
        render_fractal_threaded(render_ctx, &job);

        if (fwrite(chunk_pixels, 4, (size_t)target_width * lines, file) !=
            (size_t)target_width * lines) {
            fprintf(stderr, "error: failed to write screenshot chunk\n");
            free(chunk_pixels);
            fclose(file);
            return 0;
        }
        int progress = (y_start + lines) * 100 / target_height;
        if (state) {
            pthread_mutex_lock(&state->state_mutex);
            state->mega_screenshot_progress = progress;
            pthread_mutex_unlock(&state->state_mutex);
        }
        printf("\rprogress: %d%%", progress);
        fflush(stdout);
    }

    printf("\nmega screenshot saved.\n");

    free(chunk_pixels);
    fclose(file);
    return 1;
}

#include <pthread.h>
#include <stdlib.h>

typedef struct {
    RendererContext* render_ctx;
    AppCommonState* state;
    int target_width;
    int target_height;
    precise_float re_min;
    precise_float re_max;
    precise_float im_min;
    precise_float im_max;
    int max_iterations;
    int fractal_type;
    complex_t julia_c;
} MegaScreenshotArgs;

static void* mega_screenshot_thread_func(void* arg) {
    MegaScreenshotArgs* args = (MegaScreenshotArgs*)arg;
    int success =
        save_mega_screenshot(args->render_ctx, args->state, args->target_width, args->target_height,
                             args->re_min, args->re_max, args->im_min, args->im_max,
                             args->max_iterations, args->fractal_type, args->julia_c);
    if (args->state) {
        pthread_mutex_lock(&args->state->state_mutex);
        args->state->mega_screenshot_active = success ? 2 : 3;
        pthread_mutex_unlock(&args->state->state_mutex);
    }
    free(args);
    return NULL;
}

void save_mega_screenshot_async(RendererContext* render_ctx, AppCommonState* state,
                                int target_width, int target_height, precise_float re_min,
                                precise_float re_max, precise_float im_min, precise_float im_max,
                                int max_iterations, int fractal_type, complex_t julia_c) {
    if (state && state->mega_screenshot_active) {
        return;  // already rendering
    }

    MegaScreenshotArgs* args = (MegaScreenshotArgs*)malloc(sizeof(MegaScreenshotArgs));
    if (!args) return;

    args->render_ctx = render_ctx;
    args->state = state;
    args->target_width = target_width;
    args->target_height = target_height;
    args->re_min = re_min;
    args->re_max = re_max;
    args->im_min = im_min;
    args->im_max = im_max;
    args->max_iterations = max_iterations;
    args->fractal_type = fractal_type;
    args->julia_c = julia_c;

    if (state) {
        state->mega_screenshot_active = 1;
        state->mega_screenshot_progress = 0;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, mega_screenshot_thread_func, args) == 0) {
        pthread_detach(thread);
    } else {
        // fallback to sync
        int success =
            save_mega_screenshot(render_ctx, state, target_width, target_height, re_min, re_max,
                                 im_min, im_max, max_iterations, fractal_type, julia_c);
        if (state) {
            pthread_mutex_lock(&state->state_mutex);
            state->mega_screenshot_active = success ? 2 : 3;
            pthread_mutex_unlock(&state->state_mutex);
        }
        free(args);
    }
}

#include <string.h>

#include "stb/stb_image_write.h"

// async video recording state
static FILE* ffmpeg_pipe = NULL;
static int is_recording = 0;
static char s_log_path[512] = "";  // absolute path to video_log.txt, resolved at start

static int check_ffmpeg_installed(void) {
#ifdef _WIN32
    return system("where ffmpeg >nul 2>&1") == 0;
#else
    return system("which ffmpeg >/dev/null 2>&1") == 0;
#endif
}

int start_video_recording(int width, int height, int fps, int is_bgra_topdown, int crf,
                          const char* preset, const char* codec, int aa_level, int show_log,
                          const char* log_fontpath, int log_fontsize, const char* custom_filename,
                          int log_position, const char* log_fontcolor) {
    if (is_recording) return 0;

    if (!check_ffmpeg_installed()) {
        fprintf(stderr, "error: ffmpeg is not installed or not in PATH.\n");
        return 0;
    }

#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

    char filename[256];
    if (custom_filename && custom_filename[0] != '\0') {
        strncpy(filename, custom_filename, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    } else {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        strftime(filename, sizeof(filename), "mandelbrot_video_%Y%m%d_%H%M%S.mp4", t);
    }

    /* use a large unified command buffer to avoid truncation.
     * vf_chain alone can be ~1600 bytes with drawtext filter + long font path,
     * so 1024 is not enough. 4096 gives comfortable headroom. */
    char command[4096];
    int render_w = width * aa_level;
    int render_h = height * aa_level;

    /* vf_chain holds the -vf filter string. with drawtext it can be ~1600 bytes,
     * so size it large enough to hold both the base filter and the drawtext suffix. */
    char vf_chain[2048];
    if (is_bgra_topdown) {
        if (aa_level > 1) {
            snprintf(vf_chain, sizeof(vf_chain), "scale=%d:%d", width, height);
        } else {
            snprintf(vf_chain, sizeof(vf_chain), "crop=trunc(iw/2)*2:trunc(ih/2)*2");
        }
    } else {
        if (aa_level > 1) {
            snprintf(vf_chain, sizeof(vf_chain), "vflip,scale=%d:%d", width, height);
        } else {
            snprintf(vf_chain, sizeof(vf_chain), "vflip,crop=trunc(iw/2)*2:trunc(ih/2)*2");
        }
    }

    if (show_log) {
        /* resolve absolute path for video_log.txt so both fopen() and ffmpeg         * can find it
         * regardless of working directory. */
        char cwd[384];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(s_log_path, sizeof(s_log_path), "%s/video_log.txt", cwd);
        } else {
            strncpy(s_log_path, "video_log.txt", sizeof(s_log_path) - 1);
        }

        // initialize empty log file — each frame appends one line
        FILE* log_f = fopen(s_log_path, "w");
        if (log_f) fclose(log_f);

        // anchor y at bottom so accumulated lines grow upward (latest line stays at bottom)
        const char* x_pos;
        const char* y_pos;
        if (log_position == 1) {  // bottom-right
            x_pos = "w-text_w-10";
            y_pos = "h-text_h-10";
        } else if (log_position == 2) {  // top-left (overflow downward)
            x_pos = "10";
            y_pos = "10";
        } else if (log_position == 3) {  // top-right (overflow downward)
            x_pos = "w-text_w-10";
            y_pos = "10";
        } else {  // bottom-left default — scroll upward
            x_pos = "10";
            y_pos = "h-text_h-10";
        }

        /* build drawtext suffix separately, then append with bounds check.
         * the old strcat(vf_chain, drawtext_filter) had no size guard and could
         * silently overflow vf_chain when font paths are long. */
        char drawtext_filter[2048];
        snprintf(drawtext_filter, sizeof(drawtext_filter),
                 ",drawtext=fontfile='%s':textfile='%s':reload=1:x=%s:y=%s:fontsize=%d:"
                 "fontcolor=%s:line_spacing=2:expansion=none",
                 (log_fontpath && log_fontpath[0] != '\0') ? log_fontpath : "assets/fonts/font.ttf",
                 s_log_path, x_pos, y_pos, log_fontsize > 0 ? log_fontsize : 20,
                 (log_fontcolor && log_fontcolor[0] != '\0') ? log_fontcolor : "white");
        size_t rem = sizeof(vf_chain) - strlen(vf_chain) - 1;
        strncat(vf_chain, drawtext_filter, rem);
    }

    if (is_bgra_topdown) {
        snprintf(command, sizeof(command),
                 "ffmpeg -y -f rawvideo -vcodec rawvideo -s %dx%d -pix_fmt bgra -r %d "
                 "-i - -vf \"%s\" -c:v %s -preset %s -crf %d "
                 "-pix_fmt yuv420p \"%s\"",
                 render_w, render_h, fps, vf_chain, codec, preset, crf, filename);
    } else {
        // gpu outputs bottom-up rgba. let ffmpeg do the vertical flip and format conversion
        snprintf(command, sizeof(command),
                 "ffmpeg -y -f rawvideo -vcodec rawvideo -s %dx%d -pix_fmt rgba -r %d "
                 "-i - -vf \"%s\" -c:v %s -preset %s -crf %d "
                 "-pix_fmt yuv420p \"%s\"",
                 render_w, render_h, fps, vf_chain, codec, preset, crf, filename);
    }

#ifdef _WIN32
    ffmpeg_pipe = _popen(command, "wb");
#else
    signal(SIGPIPE, SIG_IGN);
    ffmpeg_pipe = popen(command, "w");
#endif

    if (!ffmpeg_pipe) {
        fprintf(stderr, "error: failed to start ffmpeg process. is ffmpeg installed?\n");
        return 0;
    }

    printf("started recording video to %s at %dx%d @ %dfps\n", filename, width, height, fps);
    is_recording = 1;
    return 1;
}

void append_video_frame(const uint32_t* pixels, int width, int height) {
    if (!is_recording || !ffmpeg_pipe) return;

    size_t written = fwrite(pixels, 4, (size_t)width * height, ffmpeg_pipe);
    if (written < (size_t)width * height) {
        fprintf(stderr, "error: ffmpeg pipe broken. stopping video recording.\n");
        is_recording = 0;
    }
}

void stop_video_recording(void) {
    if (!ffmpeg_pipe) return;

    // mark as not recording immediately to prevent new frames from being appended
    is_recording = 0;

#ifdef _WIN32
    _pclose(ffmpeg_pipe);
#else
    pclose(ffmpeg_pipe);
#endif

    ffmpeg_pipe = NULL;
    printf("stopped video recording.\n");

    // clean up temporary log file and reset path
    if (s_log_path[0]) {
        remove(s_log_path);
        s_log_path[0] = '\0';
    }
}

int is_video_recording(void) {
    return is_recording;
}

void save_screenshot(AppCommonState* state, const uint32_t* pixels, int width, int height,
                     uint32_t now, int is_bgra, int is_bottom_up) {
    static uint32_t counter = 0;
    char filename[256];
    snprintf(filename, sizeof(filename), "screenshot_%u_%u.png", (unsigned int)time(NULL),
             ++counter);

    uint32_t* rgba_pixels = NULL;

    // if input is bgra, we need to convert to rgba since stbi_write_png expects rgba
    if (is_bgra) {
        rgba_pixels = (uint32_t*)malloc((size_t)width * height * 4);
        if (!rgba_pixels) {
            if (state) app_state_push_notification(state, "error: screenshot failed!", now);
            return;
        }

        for (int i = 0; i < width * height; i++) {
            uint32_t p = pixels[i];
            // swap red and blue
            uint8_t b = (p >> 0) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t a = (p >> 24) & 0xFF;
            rgba_pixels[i] =
                (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
        }
    }

#if defined(__EMSCRIPTEN__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
    EM_ASM(
        {
            try {
                if (window.downloadScreenshotData) {
                    var heap = (typeof HEAPU8 !== 'undefined') ? HEAPU8 : Module.HEAPU8;
                    window.downloadScreenshotData($0, $1, $2, heap, $3);
                }
            } catch (e) {
                console.error("Screenshot error:", e);
            }
        },
        (void*)(rgba_pixels ? rgba_pixels : pixels), width, height, is_bottom_up);
#pragma clang diagnostic pop

    if (rgba_pixels) free(rgba_pixels);
    if (state) app_state_push_notification(state, "screenshot captured!", now);
    return;
#endif

    stbi_flip_vertically_on_write(is_bottom_up);

    if (stbi_write_png(filename, width, height, 4, rgba_pixels ? rgba_pixels : pixels, width * 4)) {
        printf("screenshot saved to %s\n", filename);
        if (state) {
            char buf[512];
            snprintf(buf, sizeof(buf), "Saved to %s", filename);
            app_state_push_notification(state, buf, now);
        }
    } else {
        fprintf(stderr, "error: failed to save screenshot\n");
        if (state) app_state_push_notification(state, "error: screenshot failed!", now);
    }

    if (rgba_pixels) {
        free(rgba_pixels);
    }
}

// ---------------------------------------------------------
// async background video export
// ---------------------------------------------------------

typedef struct {
    AppCommonState state_copy;
    AppCommonState* real_state;
} VideoExportArgs;

static void* video_export_thread_func(void* arg) {
    VideoExportArgs* args = (VideoExportArgs*)arg;
    AppCommonState* state = &args->state_copy;
    AppCommonState* real_state = args->real_state;

    int fps = state->video_settings.fps;
    int duration = state->video_settings.duration_sec;
    int total_frames = fps * duration;
    int aa_level =
        (state->video_settings.aa_level == 0) ? 1 : ((state->video_settings.aa_level == 1) ? 2 : 4);
    int render_w = state->video_settings.res_w * aa_level;
    int render_h = state->video_settings.res_h * aa_level;

    // start ffmpeg pipe using existing function
    const char* presets[] = {"ultrafast", "superfast", "veryfast", "faster",  "fast",
                             "medium",    "slow",      "slower",   "veryslow"};
    const char* codecs[] = {"libx264", "libx265"};

    int ok = start_video_recording(
        state->video_settings.res_w, state->video_settings.res_h, fps, 0,
        state->video_settings.crf_val, presets[state->video_settings.preset_idx],
        codecs[state->video_settings.codec_idx], aa_level, state->video_settings.show_log,
        state->video_settings.log_fontpath, state->video_settings.log_fontsize,
        state->video_settings.output_filename, state->video_settings.log_position,
        state->video_settings.log_fontcolor);

    if (!ok) {
        fprintf(stderr, "error: failed to start video recording\n");
        real_state->video_settings.is_rendering = 0;
        app_state_push_notification(real_state, "error: failed to start ffmpeg!",
                                    0);  // time doesn't matter much here
        free(args);
        return NULL;
    }

    // initialize isolated cpu renderer
    RendererContext* render_ctx = init_renderer(state->max_iterations, state->palette_idx);
    if (!render_ctx) {
        fprintf(stderr, "error: failed to init renderer\n");
        stop_video_recording();
        real_state->video_settings.is_rendering = 0;
        free(args);
        return NULL;
    }

    uint32_t* vbuf = malloc((size_t)render_w * render_h * 4);
    if (!vbuf) {
        fprintf(stderr, "error: failed to allocate frame buffer\n");
        cleanup_renderer(render_ctx);
        stop_video_recording();
        real_state->video_settings.is_rendering = 0;
        free(args);
        return NULL;
    }

    // initialize tour
    app_state_start_video_render(state, 0);

    for (int frame_idx = 0; frame_idx < total_frames; frame_idx++) {
        // check for cancellation
        if (real_state->video_settings.export_cancelled) {
            break;
        }

        uint32_t simulated_time = frame_idx * (1000 / fps);

        app_state_step_simulation(state, simulated_time);

        precise_float rmin, rmax, imin, imax;
        app_state_calculate_boundaries(state, render_w, render_h, &rmin, &rmax, &imin, &imax);

        RenderJob job = {.pixels = vbuf,
                         .pitch = render_w * 4,
                         .window_width = render_w,
                         .window_height = render_h,
                         .re_min = rmin,
                         .re_max = rmax,
                         .im_top = imax,
                         .im_bottom = imin,
                         .mode = state->julia_mode ? RENDER_JULIA : state->base_fractal,
                         .julia_c = state->julia_c,
                         .max_iterations = state->max_iterations};

        // time the render, then append one log line (accumulates — newest at bottom)
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        render_fractal_threaded(render_ctx, &job);
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        double render_ms =
            (t_end.tv_sec - t_start.tv_sec) * 1000.0 + (t_end.tv_nsec - t_start.tv_nsec) / 1e6;

        if (state->video_settings.show_log) {
            FILE* log_f = fopen(s_log_path[0] ? s_log_path : "video_log.txt", "a");
            if (log_f) {
                fprintf(log_f,
                        "[RENDER] frame %d/%d (%.1f%%) complete in %.1f ms | "
                        "Center: Re=%.8f, Im=%.8f, Zoom=%.6g\n",
                        frame_idx + 1, total_frames, (float)(frame_idx + 1) / total_frames * 100.0f,
                        render_ms, (double)state->cam.view.center_re,
                        (double)state->cam.view.center_im, (double)state->cam.view.zoom);
                fclose(log_f);
            }
        }

        // dump frame to ffmpeg pipe
        append_video_frame(vbuf, render_w, render_h);

        // update progress on real state
        real_state->video_settings.export_progress_percent =
            (float)(frame_idx + 1) / (float)total_frames;
    }

    // wait for the video worker to flush frames and close pipe
    stop_video_recording();
    cleanup_renderer(render_ctx);
    free(vbuf);

    /* take the mutex before touching real_state — the main thread reads
     * notifications and is_rendering every frame without synchronization otherwise. */
    pthread_mutex_lock(&real_state->state_mutex);
    if (real_state->video_settings.export_cancelled) {
        app_state_push_notification(real_state, "video export cancelled.", 0);
    } else {
        app_state_push_notification(real_state, "video export completed!", 0);
    }
    real_state->video_settings.is_rendering = 0;
    real_state->video_settings.export_cancelled = 0;
    pthread_mutex_unlock(&real_state->state_mutex);

    free(args);
    return NULL;
}

void start_video_export_async(AppCommonState* state) {
    if (state->video_settings.is_rendering) return;

    VideoExportArgs* args = (VideoExportArgs*)malloc(sizeof(VideoExportArgs));
    if (!args) return;

    // snapshot the current state so the export has its own independent camera and settings
    memcpy(&args->state_copy, state, sizeof(AppCommonState));
    args->real_state = state;

    state->video_settings.is_rendering = 1;
    state->video_settings.export_progress_percent = 0.0f;
    state->video_settings.export_cancelled = 0;

    pthread_t thread;
    if (pthread_create(&thread, NULL, video_export_thread_func, args) == 0) {
        pthread_detach(thread);
    } else {
        state->video_settings.is_rendering = 0;
        app_state_push_notification(state, "error: failed to start export thread!", 0);
        free(args);
    }
}
