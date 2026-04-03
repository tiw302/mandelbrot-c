#include "renderer.h"
#include "mandelbrot.h"
#include "julia.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

static Uint8 color_lut[MAX_ITERATIONS_LIMIT + 1][3];

void init_renderer(int max_iterations, int palette_idx) {
    for (int i = 0; i < max_iterations; i++) {
        switch (palette_idx) {
        case 0: // sine-wave (original)
            color_lut[i][0] = (Uint8)(sin(0.1 * i + 0) * 127 + 128);
            color_lut[i][1] = (Uint8)(sin(0.1 * i + 2) * 127 + 128);
            color_lut[i][2] = (Uint8)(sin(0.1 * i + 4) * 127 + 128);
            break;
        case 1: // grayscale
            color_lut[i][0] = color_lut[i][1] = color_lut[i][2] = (Uint8)(i % 256);
            break;
        case 2: // fire
            color_lut[i][0] = (Uint8)fmin(255, i * 4);
            color_lut[i][1] = (Uint8)fmin(255, i * 2);
            color_lut[i][2] = (Uint8)fmin(255, i * 1);
            break;
        case 3: // electric
            color_lut[i][0] = (Uint8)fmin(255, i * 1);
            color_lut[i][1] = (Uint8)fmin(255, i * 4);
            color_lut[i][2] = (Uint8)fmin(255, i * 8);
            break;
        default:
            color_lut[i][0] = color_lut[i][1] = color_lut[i][2] = 127;
            break;
        }
    }
    // points in the set are black
    color_lut[max_iterations][0] = 0;
    color_lut[max_iterations][1] = 0;
    color_lut[max_iterations][2] = 0;
}

void get_color(double iterations, int max_iterations, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (iterations >= max_iterations) {
        *r = *g = *b = 0;
        return;
    }

    if (iterations < 0) iterations = 0;

    int i = (int)iterations;
    double t = iterations - i;

    // lerp between LUT entries for smoothness
    int i2 = i + 1;
    if (i2 > max_iterations) i2 = max_iterations;

    *r = (Uint8)(color_lut[i][0] * (1.0 - t) + color_lut[i2][0] * t);
    *g = (Uint8)(color_lut[i][1] * (1.0 - t) + color_lut[i2][1] * t);
    *b = (Uint8)(color_lut[i][2] * (1.0 - t) + color_lut[i2][2] * t);
}

void *render_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    double re_factor = (data->re_max - data->re_min) / data->window_width;
    double im_factor = (data->im_max - data->im_min) / data->window_height;

    // dynamic row assignment via atomic queue
    int y;
    while ((y = atomic_fetch_add(data->next_row, 1)) < data->window_height) {
        int x = 0;

#ifdef __AVX2__
        // process 4 pixels at a time with AVX2
        for (; x <= data->window_width - 4; x += 4) {
            double res_re[4], res_im[4], iterations[4];
            for (int i = 0; i < 4; i++) {
                res_re[i] = data->re_min + (double)(x + i) * re_factor;
                res_im[i] = data->im_min + (double)y * im_factor;
            }

            if (data->mode == RENDER_JULIA)
                julia_check_avx2(res_re, res_im, data->julia_c, data->max_iterations, iterations);
            else
                mandelbrot_check_avx2(res_re, res_im, data->max_iterations, iterations);

            for (int i = 0; i < 4; i++) {
                Uint8 r, g, b;
                get_color(iterations[i], data->max_iterations, &r, &g, &b);
                data->pixels[y * (data->pitch / sizeof(Uint32)) + (x + i)] =
                    (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
#endif

        // remaining pixels
        for (; x < data->window_width; x++) {
            complex_t point;
            point.re = data->re_min + (double)x * re_factor;
            point.im = data->im_min + (double)y * im_factor;

            double iterations;
            if (data->mode == RENDER_JULIA)
                iterations = julia_check(point, data->julia_c, data->max_iterations);
            else
                iterations = mandelbrot_check(point, data->max_iterations);

            Uint8 r, g, b;
            get_color(iterations, data->max_iterations, &r, &g, &b);

            // write to ARGB pixel buffer
            data->pixels[y * (data->pitch / sizeof(Uint32)) + x] =
                (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    return NULL;
}

static void dispatch_threads(Uint32 *pixels, int pitch,
                              int window_width, int window_height,
                              double re_min, double re_max,
                              double im_min, double im_max,
                              RenderMode mode, complex_t julia_c,
                              int max_iterations) {
    pthread_t      threads[THREAD_COUNT];
    thread_data_t  thread_data[THREAD_COUNT];
    atomic_int     next_row = 0;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i] = (thread_data_t){
            .id = i,
            .pixels = pixels,
            .pitch = pitch,
            .window_width = window_width,
            .window_height = window_height,
            .re_min = re_min,
            .re_max = re_max,
            .im_min = im_min,
            .im_max = im_max,
            .mode = mode,
            .julia_c = julia_c,
            .max_iterations = max_iterations,
            .next_row = &next_row
        };

        if (pthread_create(&threads[i], NULL, render_thread, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to spawn thread %d\n", i);
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            return;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) pthread_join(threads[i], NULL);
}

void render_mandelbrot_threaded(Uint32 *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max,
                                int max_iterations) {
    complex_t dummy = {0};
    dispatch_threads(pixels, pitch, window_width, window_height,
                     re_min, re_max, im_min, im_max,
                     RENDER_MANDELBROT, dummy, max_iterations);
}

void render_julia_threaded(Uint32 *pixels, int pitch,
                           int window_width, int window_height,
                           double re_min, double re_max,
                           double im_min, double im_max,
                           complex_t julia_c,
                           int max_iterations) {
    dispatch_threads(pixels, pitch, window_width, window_height,
                     re_min, re_max, im_min, im_max,
                     RENDER_JULIA, julia_c, max_iterations);
}
