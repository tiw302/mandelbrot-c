#include "screenshot.h"
// screenshot.c is the sole owner of stb_image_write implementation.
// do not define STB_IMAGE_WRITE_IMPLEMENTATION anywhere else.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "renderer.h"

void save_mega_screenshot(int target_width, int target_height, double re_min, double re_max,
                          double im_min, double im_max, int max_iterations, int palette_idx,
                          int fractal_type, complex_t julia_c) {
    char filename[128];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(filename, sizeof(filename), "mega_mandelbrot_%Y%m%d_%H%M%S.tga", t);

    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "error: failed to open mega screenshot file for writing\n");
        return;
    }

    // write tga header (uncompressed true-color image)
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
     * say, 256 lines at a time. */
    int chunk_height = 256;
    if (chunk_height > target_height) chunk_height = target_height;

    uint32_t* chunk_pixels = (uint32_t*)malloc((size_t)target_width * chunk_height * 4);
    if (!chunk_pixels) {
        fprintf(stderr, "error: failed to allocate memory for mega screenshot chunk\n");
        fclose(file);
        return;
    }

    double im_step = (im_max - im_min) / target_height;
    int pitch = target_width * 4;

    // initialize renderer palette just in case
    init_renderer(max_iterations, palette_idx);

    printf("saving mega screenshot (%dx%d) to %s...\n", target_width, target_height, filename);

    for (int y_start = 0; y_start < target_height; y_start += chunk_height) {
        int lines = chunk_height;
        if (y_start + lines > target_height) {
            lines = target_height - y_start;
        }

        double strip_im_max = im_max - y_start * im_step;
        double strip_im_min = strip_im_max - lines * im_step;

        if (fractal_type == 1) {
            render_julia_threaded(chunk_pixels, pitch, target_width, lines, re_min, re_max,
                                  strip_im_max, strip_im_min, julia_c, max_iterations);
        } else if (fractal_type == 2) {
            render_burning_ship_threaded(chunk_pixels, pitch, target_width, lines, re_min, re_max,
                                         strip_im_max, strip_im_min, max_iterations);
        } else {
            render_mandelbrot_threaded(chunk_pixels, pitch, target_width, lines, re_min, re_max,
                                       strip_im_max, strip_im_min, max_iterations);
        }

        fwrite(chunk_pixels, 4, (size_t)target_width * lines, file);
        printf("\rprogress: %d%%", (y_start + lines) * 100 / target_height);
        fflush(stdout);
    }

    printf("\nmega screenshot saved.\n");

    free(chunk_pixels);
    fclose(file);
}

#include <pthread.h>
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
            if (ffmpeg_pipe) fwrite(frame_data, 4, (size_t)video_w * video_h, ffmpeg_pipe);
            free(frame_data);
        } else if (video_shutdown) {
            break;
        }
    }
    return NULL;
}

int start_video_recording(int width, int height, int fps) {
    if (is_recording) return 0;

    char filename[128];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(filename, sizeof(filename), "mandelbrot_video_%Y%m%d_%H%M%S.mp4", t);

    char command[512];
    /* we pipe raw rgb32 (bgra on little endian) data to ffmpeg.
     * using ultrafast preset and crf 18 for good quality without bottlenecking the app too much. */
    snprintf(command, sizeof(command),
             "ffmpeg -y -f rawvideo -vcodec rawvideo -s %dx%d -pix_fmt bgra -r %d "
             "-i - -vf \"crop=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -preset ultrafast -crf 18 "
             "-pix_fmt yuv420p \"%s\"",
             width, height, fps, filename);

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

void append_video_frame(uint32_t* pixels, int width, int height) {
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
    is_recording = 0;
    printf("stopped video recording.\n");
}

int is_video_recording(void) {
    return is_recording;
}

void save_screenshot(uint32_t* pixels, int width, int height) {
    char filename[64];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(filename, sizeof(filename), "mandelbrot_%Y%m%d_%H%M%S.png", t);

    // allocate temp buffer for rgba conversion
    uint32_t* rgba_pixels = (uint32_t*)malloc((size_t)width * height * 4);
    if (!rgba_pixels) return;

    for (int i = 0; i < width * height; i++) {
        uint32_t p = pixels[i];
        // swap red (bit 16-23) and blue (bit 0-7)
        uint8_t b = (p >> 0) & 0xFF;
        uint8_t g = (p >> 8) & 0xFF;
        uint8_t r = (p >> 16) & 0xFF;
        uint8_t a = (p >> 24) & 0xFF;
        rgba_pixels[i] =
            (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    }

    if (stbi_write_png(filename, width, height, 4, rgba_pixels, width * 4)) {
        printf("screenshot saved to %s\n", filename);
    } else {
        fprintf(stderr, "error: failed to save screenshot\n");
    }

    free(rgba_pixels);
}