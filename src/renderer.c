#include "renderer.h"
#include "mandelbrot.h"
#include "julia.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <math.h>

/* Global Look-Up Table for iteration-to-color mapping */
static Uint8 color_lut[MAX_ITERATIONS + 1][3];

void init_renderer(void) {
    /*
     * Pre-calculate the sin-wave palette.
     * Points inside the set (iterations == MAX_ITERATIONS) are black.
     */
    double freq = 0.1;
    for (int i = 0; i < MAX_ITERATIONS; i++) {
        color_lut[i][0] = (Uint8)(sin(freq * i + 0) * 127 + 128);
        color_lut[i][1] = (Uint8)(sin(freq * i + 2) * 127 + 128);
        color_lut[i][2] = (Uint8)(sin(freq * i + 4) * 127 + 128);
    }
    /* Inside set */
    color_lut[MAX_ITERATIONS][0] = 0;
    color_lut[MAX_ITERATIONS][1] = 0;
    color_lut[MAX_ITERATIONS][2] = 0;
}

/*
 * Maps an iteration count to an RGB colour using the pre-calculated LUT.
 */
void get_color(int iterations, Uint8 *r, Uint8 *g, Uint8 *b) {
    /* Boundary check just in case, though MAX_ITERATIONS is the cap */
    if (iterations < 0) iterations = 0;
    if (iterations > MAX_ITERATIONS) iterations = MAX_ITERATIONS;

    *r = color_lut[iterations][0];
    *g = color_lut[iterations][1];
    *b = color_lut[iterations][2];
}

#include <stdatomic.h>

/*
 * Thread entry point. Renders the next available row from the work queue.
 *
 * Branches on data->mode to choose between Mandelbrot and Julia iteration.
 * The arithmetic is identical; only the starting conditions differ.
 */
void *render_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    /* How much of the complex plane each pixel covers */
    double re_factor = (data->re_max - data->re_min) / data->window_width;
    double im_factor = (data->im_max - data->im_min) / data->window_height;

    int y;
    while ((y = atomic_fetch_add(data->next_row, 1)) < data->window_height) {
        for (int x = 0; x < data->window_width; x++) {
            complex_t point;
            point.re = data->re_min + (double)x * re_factor;
            point.im = data->im_min + (double)y * im_factor;

            int iterations;
            if (data->mode == RENDER_JULIA)
                iterations = julia_check(point, data->julia_c);
            else
                iterations = mandelbrot_check(point);

            Uint8 r, g, b;
            get_color(iterations, &r, &g, &b);

            /* Pack into ARGB8888 format (alpha always fully opaque) */
            data->pixels[y * (data->pitch / sizeof(Uint32)) + x] =
                (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    return NULL;
}

/*
 * Internal helper: spawns THREAD_COUNT threads and uses an atomic counter
 * to dynamically distribute rows among them.
 */
static void dispatch_threads(Uint32 *pixels, int pitch,
                              int window_width, int window_height,
                              double re_min, double re_max,
                              double im_min, double im_max,
                              RenderMode mode, complex_t julia_c) {
    pthread_t      threads[THREAD_COUNT];
    thread_data_t  thread_data[THREAD_COUNT];
    atomic_int     next_row = 0;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].id            = i;
        thread_data[i].pixels        = pixels;
        thread_data[i].pitch         = pitch;
        thread_data[i].window_width  = window_width;
        thread_data[i].window_height = window_height;
        thread_data[i].re_min        = re_min;
        thread_data[i].re_max        = re_max;
        thread_data[i].im_min        = im_min;
        thread_data[i].im_max        = im_max;
        thread_data[i].mode          = mode;
        thread_data[i].julia_c       = julia_c;
        thread_data[i].next_row      = &next_row;

        if (pthread_create(&threads[i], NULL, render_thread, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            for (int j = 0; j < i; j++)
                pthread_join(threads[j], NULL);
            return;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++)
        pthread_join(threads[i], NULL);
}

/* Public wrappers --------------------------------------------------------- */

void render_mandelbrot_threaded(Uint32 *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max) {
    complex_t dummy = {0.0, 0.0};  /* julia_c ignored in Mandelbrot mode */
    dispatch_threads(pixels, pitch, window_width, window_height,
                     re_min, re_max, im_min, im_max,
                     RENDER_MANDELBROT, dummy);
}

void render_julia_threaded(Uint32 *pixels, int pitch,
                           int window_width, int window_height,
                           double re_min, double re_max,
                           double im_min, double im_max,
                           complex_t julia_c) {
    dispatch_threads(pixels, pitch, window_width, window_height,
                     re_min, re_max, im_min, im_max,
                     RENDER_JULIA, julia_c);
}
