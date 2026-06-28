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
// screenshot.c is the sole owner of stb_image_write implementation.
// do not define STB_IMAGE_WRITE_IMPLEMENTATION anywhere else.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if !defined(_WIN32)
#include <signal.h>
#endif

#include "renderer.h"

int save_mega_screenshot(RendererContext* render_ctx, AppCommonState* state, int target_width, int target_height, precise_float re_min,
                          precise_float re_max, precise_float im_min, precise_float im_max,
                          int max_iterations, int palette_idx, int fractal_type,
                          complex_t julia_c) {
    static uint32_t mega_counter = 0;
    char filename[256];
    snprintf(filename, sizeof(filename), "mega_screenshot_%u_%u.tga", (unsigned int)time(NULL), ++mega_counter);

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "error: failed to open mega screenshot file for writing\n");
        return 0;
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
    fwrite(header, 1, 18, file);

    /* we render in chunks (strips) to save memory.
     * by allocating a small 256-line buffer instead of the whole target_height,
     * we bound memory consumption to under 8mb even for massive resolution targets. */
    int chunk_height = 256;
    if (chunk_height > target_height) chunk_height = target_height;

    uint32_t* chunk_pixels = (uint32_t*)malloc((size_t)target_width * chunk_height * 4);
    if (!chunk_pixels) {
        fprintf(stderr, "error: failed to allocate memory for mega screenshot chunk\n");
        fclose(file);
        return 0;
    }

    precise_float im_step = (im_max - im_min) / target_height;
    int pitch = target_width * 4;

    // initialize renderer palette just in case
    init_color_palette(max_iterations, palette_idx);

    printf("saving mega screenshot (%dx%d) to %s...\n", target_width, target_height, filename);

    for (int y_start = 0; y_start < target_height; y_start += chunk_height) {
        int lines = chunk_height;
        if (y_start + lines > target_height) {
            lines = target_height - y_start;
        }

        precise_float strip_im_max = im_max - y_start * im_step;
        precise_float strip_im_min = strip_im_max - lines * im_step;

        if (fractal_type == 1) {
            render_julia_threaded(render_ctx, chunk_pixels, pitch, target_width, lines, re_min, re_max,
                                  strip_im_max, strip_im_min, julia_c, max_iterations);
        } else if (fractal_type == 2) {
            render_burning_ship_threaded(render_ctx, chunk_pixels, pitch, target_width, lines, re_min, re_max,
                                         strip_im_max, strip_im_min, max_iterations);
        } else {
            render_mandelbrot_threaded(render_ctx, chunk_pixels, pitch, target_width, lines, re_min, re_max,
                                       strip_im_max, strip_im_min, max_iterations);
        }

        fwrite(chunk_pixels, 4, (size_t)target_width * lines, file);
        int progress = (y_start + lines) * 100 / target_height;
        if (state) {
            state->mega_screenshot_progress = progress;
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
    int palette_idx;
    int fractal_type;
    complex_t julia_c;
} MegaScreenshotArgs;

static void* mega_screenshot_thread_func(void* arg) {
    MegaScreenshotArgs* args = (MegaScreenshotArgs*)arg;
    int success = save_mega_screenshot(args->render_ctx, args->state, args->target_width, args->target_height, args->re_min,
                                       args->re_max, args->im_min, args->im_max, args->max_iterations,
                                       args->palette_idx, args->fractal_type, args->julia_c);
    if (args->state) {
        args->state->mega_screenshot_active = success ? 2 : 3;
    }
    free(args);
    return NULL;
}

void save_mega_screenshot_async(RendererContext* render_ctx, AppCommonState* state, int target_width, int target_height, precise_float re_min,
                                precise_float re_max, precise_float im_min, precise_float im_max,
                                int max_iterations, int palette_idx, int fractal_type,
                                complex_t julia_c) {
    if (state && state->mega_screenshot_active) {
        return; // already rendering
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
    args->palette_idx = palette_idx;
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
        int success = save_mega_screenshot(render_ctx, state, target_width, target_height, re_min, re_max, im_min, im_max,
                                           max_iterations, palette_idx, fractal_type, julia_c);
        if (state) {
            state->mega_screenshot_active = success ? 2 : 3;
        }
        free(args);
    }
}

#include <string.h>

#include "stb/stb_image_write.h"

// async video recording state
static FILE* ffmpeg_pipe = NULL;
static int is_recording = 0;

// simple ring buffer to park frames and prevent ui stalls (drops frames if full)
#define VIDEO_QUEUE_SIZE 8
static uint32_t* video_queue[VIDEO_QUEUE_SIZE];
static int queue_head = 0, queue_tail = 0, queue_count = 0;
static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t video_cond = PTHREAD_COND_INITIALIZER;
static pthread_t video_thread;
static int video_w = 0, video_h = 0;
static volatile int video_shutdown = 0;

// background worker to push frames into ffmpeg pipe
static void* video_worker(void* arg) {
    (void)arg;
    while (1) {
        uint32_t* frame_data = NULL;
        pthread_mutex_lock(&video_mutex);
        while (queue_count == 0 && !video_shutdown) {
            pthread_cond_wait(&video_cond, &video_mutex);
        }
        if (queue_count > 0) {
            frame_data = video_queue[queue_tail];
            queue_tail = (queue_tail + 1) % VIDEO_QUEUE_SIZE;
            queue_count--;
        }
        pthread_mutex_unlock(&video_mutex);

        if (frame_data) {
            if (ffmpeg_pipe) {
                size_t written = fwrite(frame_data, 4, (size_t)video_w * video_h, ffmpeg_pipe);
                if (written < (size_t)video_w * video_h) {
                    fprintf(stderr, "error: ffmpeg pipe broken. stopping video recording.\n");
                    is_recording = 0;
                    video_shutdown = 1;
                }
            }
            free(frame_data);
        } else if (video_shutdown) {
            break;
        }
    }
    return NULL;
}

static int check_ffmpeg_installed(void) {
#ifdef _WIN32
    return system("where ffmpeg >nul 2>&1") == 0;
#else
    return system("which ffmpeg >/dev/null 2>&1") == 0;
#endif
}

int start_video_recording(int width, int height, int fps, int is_bgra_topdown) {
    if (is_recording) return 0;

    if (!check_ffmpeg_installed()) {
        fprintf(stderr, "error: ffmpeg is not installed or not in PATH.\n");
        return 0;
    }

#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

    char filename[128];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(filename, sizeof(filename), "mandelbrot_video_%Y%m%d_%H%M%S.mp4", t);

    char command[512];
    // use ultrafast preset and crf 18 for good quality without bottlenecking the app too much.
    if (is_bgra_topdown) {
        snprintf(command, sizeof(command),
                 "ffmpeg -y -f rawvideo -vcodec rawvideo -s %dx%d -pix_fmt bgra -r %d "
                 "-i - -vf \"crop=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -preset ultrafast -crf 18 "
                 "-pix_fmt yuv420p \"%s\"",
                 width, height, fps, filename);
    } else {
        // gpu outputs bottom-up rgba. let ffmpeg do the vertical flip and format conversion
        snprintf(command, sizeof(command),
                 "ffmpeg -y -f rawvideo -vcodec rawvideo -s %dx%d -pix_fmt rgba -r %d "
                 "-i - -vf \"vflip,crop=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -preset ultrafast -crf 18 "
                 "-pix_fmt yuv420p \"%s\"",
                 width, height, fps, filename);
    }

#ifdef _WIN32
    ffmpeg_pipe = _popen(command, "wb");
#else
    ffmpeg_pipe = popen(command, "w");
#endif

    if (!ffmpeg_pipe) {
        fprintf(stderr, "error: failed to start ffmpeg process. is ffmpeg installed?\n");
        return 0;
    }

    // setup queue and start worker thread
    video_w = width;
    video_h = height;
    video_shutdown = 0;
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    if (pthread_create(&video_thread, NULL, video_worker, NULL) != 0) {
        fprintf(stderr, "error: failed to spawn video worker thread\n");
#ifdef _WIN32
        _pclose(ffmpeg_pipe);
#else
        pclose(ffmpeg_pipe);
#endif
        ffmpeg_pipe = NULL;
        return 0;
    }

    printf("started recording video to %s at %dx%d @ %dfps\n", filename, width, height, fps);
    is_recording = 1;
    return 1;
}

void append_video_frame(const uint32_t* pixels, int width, int height) {
    if (!is_recording || !ffmpeg_pipe) return;

    // copy frame to memory and queue it for the background thread
    uint32_t* frame_copy = malloc((size_t)width * height * 4);
    if (!frame_copy) return;
    memcpy(frame_copy, pixels, (size_t)width * height * 4);

    pthread_mutex_lock(&video_mutex);
    if (queue_count < VIDEO_QUEUE_SIZE) {
        video_queue[queue_head] = frame_copy;
        queue_head = (queue_head + 1) % VIDEO_QUEUE_SIZE;
        queue_count++;
        pthread_cond_signal(&video_cond);
    } else {
        // drop frame if disk/ffmpeg can't keep up to prevent memory bloat
        free(frame_copy);
        fprintf(stderr, "warning: video queue full, dropping frame\n");
    }
    pthread_mutex_unlock(&video_mutex);
}

void stop_video_recording(void) {
    if (!is_recording || !ffmpeg_pipe) return;

    // mark as not recording immediately to prevent new frames from being appended
    is_recording = 0;

    // signal worker to stop and wait for it to clear the queue
    pthread_mutex_lock(&video_mutex);
    video_shutdown = 1;
    pthread_cond_signal(&video_cond);
    pthread_mutex_unlock(&video_mutex);
    pthread_join(video_thread, NULL);

    // worker drains the queue completely before exiting,
    // so no manual cleanup of video_queue is needed here.

#ifdef _WIN32
    _pclose(ffmpeg_pipe);
#else
    pclose(ffmpeg_pipe);
#endif

    ffmpeg_pipe = NULL;
    printf("stopped video recording.\n");
}

int is_video_recording(void) {
    return is_recording;
}

void save_screenshot(AppCommonState* state, const uint32_t* pixels, int width, int height, uint32_t now, int is_bgra, int is_bottom_up) {
    static uint32_t counter = 0;
    char filename[256];
    snprintf(filename, sizeof(filename), "screenshot_%u_%u.png", (unsigned int)time(NULL), ++counter);

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
            rgba_pixels[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
        }
    }

    stbi_flip_vertically_on_write(is_bottom_up);

    if (stbi_write_png(filename, width, height, 4, rgba_pixels ? rgba_pixels : pixels, width * 4)) {
        printf("screenshot saved to %s\n", filename);
        if (state) app_state_push_notification(state, "screenshot saved!", now);
    } else {
        fprintf(stderr, "error: failed to save screenshot\n");
        if (state) app_state_push_notification(state, "error: screenshot failed!", now);
    }

    if (rgba_pixels) {
        free(rgba_pixels);
    }
}
