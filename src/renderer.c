#include "renderer.h"
#include "mandelbrot.h"
#include "julia.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdatomic.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__) || defined(__MACH__) || defined(__linux__) || defined(__FreeBSD__)
#include <unistd.h>
#endif

static Uint8 color_lut[MAX_ITERATIONS_LIMIT + 1][3];
static int   actual_thread_count       = 1;
static int   thread_count_initialized  = 0;

const char *PALETTE_NAMES[PALETTE_COUNT] = {
    "Sine Wave", "Grayscale", "Fire", "Electric", "Ocean", "Inferno"
};

static int get_cpu_cores(void) {
    int cores = 1;
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    cores = (int)sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    long result = sysconf(_SC_NPROCESSORS_ONLN);
    if (result > 0)
        cores = (int)result;
#endif
    return cores;
}

void init_renderer(int max_iterations, int palette_idx) {
    // Only detect once at startup. Using a flag instead of checking the count
    // value avoids misidentifying a legitimate single-core result as uninitialised.
    if (!thread_count_initialized) {
        int cores = get_cpu_cores();
        actual_thread_count = (DEFAULT_THREAD_COUNT > 0) ? DEFAULT_THREAD_COUNT : cores;
        if (actual_thread_count < 1)  actual_thread_count = 1;
        if (actual_thread_count > 64) actual_thread_count = 64;
        thread_count_initialized = 1;
        printf("[renderer] detected %d CPU core(s), using %d thread(s)\n",
               cores, actual_thread_count);
    }

    for (int i = 0; i < max_iterations; i++) {
        switch (palette_idx) {
        case 0: // sine wave
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
        case 4: // ocean
            color_lut[i][0] = (Uint8)fmin(255, i * 0.5);
            color_lut[i][1] = (Uint8)fmin(255, i * 2);
            color_lut[i][2] = (Uint8)fmin(255, i * 5);
            break;
        case 5: // inferno
            color_lut[i][0] = (Uint8)fmin(255, i * 8);
            color_lut[i][1] = (Uint8)fmin(255, i * 2);
            color_lut[i][2] = (Uint8)fmin(255, i * 0.5);
            break;
        default:
            color_lut[i][0] = color_lut[i][1] = color_lut[i][2] = 127;
            break;
        }
    }
    // set points are black
    color_lut[max_iterations][0] = 0;
    color_lut[max_iterations][1] = 0;
    color_lut[max_iterations][2] = 0;
}

int get_actual_thread_count(void) {
    return actual_thread_count;
}

void get_color(double iterations, int max_iterations, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (iterations >= max_iterations) {
        *r = *g = *b = 0;
        return;
    }

    if (iterations < 0) iterations = 0;

    int i = (int)iterations;
    double t = iterations - i;

    // interpolate colors for smoothness
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

    // assign rows dynamically
    int y;
    while ((y = atomic_fetch_add(data->next_row, 1)) < data->window_height) {
        int x = 0;

#ifdef __AVX2__
        // calculate vectors
        __m256d v_re_min = _mm256_set1_pd(data->re_min);
        __m256d v_re_fac = _mm256_set1_pd(re_factor);
        __m256d v_im_val = _mm256_set1_pd(data->im_min + (double)y * im_factor);
        __m256d v_offsets = _mm256_set_pd(3.0, 2.0, 1.0, 0.0);

        // process 4 pixels with AVX2
        for (; x <= data->window_width - 4; x += 4) {
            double res_re[4], res_im[4], iterations[4];

            __m256d v_x = _mm256_add_pd(_mm256_set1_pd((double)x), v_offsets);
            __m256d v_re = _mm256_add_pd(v_re_min, _mm256_mul_pd(v_x, v_re_fac));

            _mm256_storeu_pd(res_re, v_re);
            _mm256_storeu_pd(res_im, v_im_val);

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

        // process remaining pixels
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

            // write to pixel buffer
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
    pthread_t      *threads     = malloc(sizeof(pthread_t) * actual_thread_count);
    thread_data_t  *thread_data = malloc(sizeof(thread_data_t) * actual_thread_count);
    atomic_int      next_row    = 0;

    if (!threads || !thread_data) {
        fprintf(stderr, "Failed to allocate thread resources\n");
        free(threads); free(thread_data);
        return;
    }

    for (int i = 0; i < actual_thread_count; i++) {
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
            free(threads); free(thread_data);
            return;
        }
    }

    for (int i = 0; i < actual_thread_count; i++) pthread_join(threads[i], NULL);

    free(threads);
    free(thread_data);
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
