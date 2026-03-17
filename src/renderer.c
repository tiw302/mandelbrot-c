#include "renderer.h"
#include "mandelbrot.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <math.h>

/*
 * Maps an iteration count to an RGB color.
 *
 * Points inside the set (iterations == MAX_ITERATIONS) are black.
 * Escaping points are colored using sine waves offset by phase so that
 * R, G, and B cycle at slightly different rates, producing smooth gradients.
 *
 * Tweak `freq` or the phase offsets (+0, +2, +4) to change the palette.
 */
void get_color(int iterations, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (iterations == MAX_ITERATIONS) {
        *r = *g = *b = 0;
        return;
    }

    /* sin() returns -1..1; scale to 0..255 */
    double freq = 0.1;
    *r = (Uint8)(sin(freq * iterations + 0) * 127 + 128);
    *g = (Uint8)(sin(freq * iterations + 2) * 127 + 128);
    *b = (Uint8)(sin(freq * iterations + 4) * 127 + 128);
}

/*
 * Thread entry point. Renders the horizontal band of rows
 * defined by data->start_y .. data->end_y (exclusive).
 */
void *render_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    /* How much of the complex plane each pixel covers */
    double re_factor = (data->re_max - data->re_min) / data->window_width;
    double im_factor = (data->im_max - data->im_min) / data->window_height;

    for (int y = data->start_y; y < data->end_y; y++) {
        for (int x = 0; x < data->window_width; x++) {
            complex_t c;
            c.re = data->re_min + (double)x * re_factor;
            c.im = data->im_min + (double)y * im_factor;

            int iterations = mandelbrot_check(c);

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
 * Renders the Mandelbrot set into `pixels` using THREAD_COUNT parallel threads.
 * Each thread handles an equal horizontal band; any leftover rows go to the last thread.
 *
 * Note: this uses CPU threads (pthreads), not GPU compute.
 */
void render_mandelbrot_threaded(Uint32 *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max) {
    pthread_t       threads[THREAD_COUNT];
    thread_data_t   thread_data[THREAD_COUNT];

    int rows_per_thread = window_height / THREAD_COUNT;
    int remaining_rows  = window_height % THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_data[i].id            = i;
        thread_data[i].pixels        = pixels;
        thread_data[i].pitch         = pitch;
        thread_data[i].start_y       = i * rows_per_thread;
        thread_data[i].end_y         = thread_data[i].start_y + rows_per_thread;
        thread_data[i].window_width  = window_width;
        thread_data[i].window_height = window_height;
        thread_data[i].re_min        = re_min;
        thread_data[i].re_max        = re_max;
        thread_data[i].im_min        = im_min;
        thread_data[i].im_max        = im_max;

        /* Give any leftover rows to the last thread */
        if (i == THREAD_COUNT - 1)
            thread_data[i].end_y += remaining_rows;

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
