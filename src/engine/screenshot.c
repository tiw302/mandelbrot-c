#include "screenshot.h"
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

    /* write tga header (uncompressed true-color image) */
    uint8_t header[18] = {0};
    header[2] = 2; /* uncompressed rgb */
    header[12] = (target_width & 0xFF);
    header[13] = (target_width >> 8);
    header[14] = (target_height & 0xFF);
    header[15] = (target_height >> 8);
    header[16] = 32; /* 32 bits per pixel */
    header[17] = 0x20; /* top-down origin */
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

    /* initialize renderer palette just in case */
    init_renderer(max_iterations, palette_idx);

    printf("saving mega screenshot (%dx%d) to %s...\n", target_width, target_height, filename);

    for (int y_start = 0; y_start < target_height; y_start += chunk_height) {
        int lines = chunk_height;
        if (y_start + lines > target_height) {
            lines = target_height - y_start;
        }

        double strip_im_min = im_min + y_start * im_step;
        double strip_im_max = strip_im_min + lines * im_step;

        if (fractal_type == 1) {
            render_julia_threaded(chunk_pixels, pitch, target_width, lines, re_min, re_max, 
                                  strip_im_min, strip_im_max, julia_c, max_iterations);
        } else if (fractal_type == 2) {
            render_burning_ship_threaded(chunk_pixels, pitch, target_width, lines, re_min, re_max, 
                                         strip_im_min, strip_im_max, max_iterations);
        } else {
            render_mandelbrot_threaded(chunk_pixels, pitch, target_width, lines, re_min, re_max, 
                                       strip_im_min, strip_im_max, max_iterations);
        }

        fwrite(chunk_pixels, 4, (size_t)target_width * lines, file);
        printf("\rprogress: %d%%", (y_start + lines) * 100 / target_height);
        fflush(stdout);
    }
    
    printf("\nmega screenshot saved.\n");

    free(chunk_pixels);
    fclose(file);
}

#include "stb/stb_image_write.h"

/* video recording state */
static FILE* ffmpeg_pipe = NULL;
static int is_recording = 0;

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
             "-i - -c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p \"%s\"",
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

    printf("started recording video to %s at %dx%d @ %dfps\n", filename, width, height, fps);
    is_recording = 1;
    return 1;
}

void append_video_frame(uint32_t* pixels, int width, int height) {
    if (!is_recording || !ffmpeg_pipe) return;
    
    /* directly write the raw pixel buffer (bgra format in memory). */
    size_t written = fwrite(pixels, 4, (size_t)width * height, ffmpeg_pipe);
    if (written != (size_t)width * height) {
        fprintf(stderr, "warning: failed to write full frame to ffmpeg pipe\n");
    }
}

void stop_video_recording(void) {
    if (!is_recording || !ffmpeg_pipe) return;

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

    /* allocate temp buffer for rgba conversion */
    uint32_t* rgba_pixels = (uint32_t*)malloc((size_t)width * height * 4);
    if (!rgba_pixels) return;

    for (int i = 0; i < width * height; i++) {
        uint32_t p = pixels[i];
        /* swap red (bit 16-23) and blue (bit 0-7) */
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