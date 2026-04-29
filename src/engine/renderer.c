#include "renderer.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "julia.h"
#include "mandelbrot.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/threading.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__) || defined(__MACH__) || defined(__linux__) || defined(__FreeBSD__)
#include <unistd.h>
#endif

static int actual_thread_count = 0;
static pthread_t* threads_pool = NULL;
static thread_data_t* thread_data_pool = NULL;

static int get_cpu_cores(void) {
#if defined(__EMSCRIPTEN__)
    return 1;
#elif defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    return sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}

static int detect_thread_count(void) {
    int cores = get_cpu_cores();
    int count = (DEFAULT_THREAD_COUNT > 0) ? DEFAULT_THREAD_COUNT : cores;
    if (count < 1) count = 1;
    if (count > 64) count = 64;
    return count;
}

int get_optimal_thread_count(void) {
    return detect_thread_count();
}

void init_renderer(int max_iterations, int palette_idx) {
    if (actual_thread_count == 0) {
        actual_thread_count = detect_thread_count();
        threads_pool = malloc(sizeof(pthread_t) * actual_thread_count);
        thread_data_pool = malloc(sizeof(thread_data_t) * actual_thread_count);
        if (!threads_pool || !thread_data_pool) {
            fprintf(stderr, "fatal: failed to allocate thread pool\n");
            if (threads_pool) free(threads_pool);
            if (thread_data_pool) free(thread_data_pool);
            exit(1);
        }
    }
    init_color_palette(max_iterations, palette_idx);
}

void cleanup_renderer(void) {
    if (threads_pool) free(threads_pool);
    if (thread_data_pool) free(thread_data_pool);
    threads_pool = NULL;
    thread_data_pool = NULL;
    actual_thread_count = 0;
}

int get_actual_thread_count(void) {
    return actual_thread_count;
}

void* render_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;

    double re_factor =
        (data->window_width > 0) ? (data->re_max - data->re_min) / data->window_width : 0;
    double im_factor =
        (data->window_height > 0) ? (data->im_max - data->im_min) / data->window_height : 0;

    int y;
    while ((y = atomic_fetch_add(data->next_row, 1)) < data->window_height) {
        int x = 0;

#ifdef __AVX2__
        __m256d v_re_min = _mm256_set1_pd(data->re_min);
        __m256d v_re_fac = _mm256_set1_pd(re_factor);
        __m256d v_im_val = _mm256_set1_pd(data->im_min + (double)y * im_factor);
        __m256d v_offsets = _mm256_set_pd(3.0, 2.0, 1.0, 0.0);

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
                uint8_t r, g, b;
                get_color(iterations[i], data->max_iterations, &r, &g, &b);
#if defined(__EMSCRIPTEN__)
                /* web/wasm expects rgba (red at byte 0) */
                data->pixels[y * (data->pitch / sizeof(uint32_t)) + (x + i)] =
                    (0xFF << 24) | (b << 16) | (g << 8) | r;
#else
                /* desktop sdl argb8888 on little endian is bgra (blue at byte 0) */
                data->pixels[y * (data->pitch / sizeof(uint32_t)) + (x + i)] =
                    (0xFF << 24) | (r << 16) | (g << 8) | b;
#endif
            }
        }
#elif defined(__wasm_simd128__)
        for (; x <= data->window_width - 2; x += 2) {
            double res_re[2], res_im[2], iterations[2];
            res_re[0] = data->re_min + (double)x * re_factor;
            res_re[1] = data->re_min + (double)(x + 1) * re_factor;
            res_im[0] = res_im[1] = data->im_min + (double)y * im_factor;

            if (data->mode == RENDER_JULIA)
                julia_check_wasm_simd128(res_re, res_im, data->julia_c, data->max_iterations,
                                         iterations);
            else
                mandelbrot_check_wasm_simd128(res_re, res_im, data->max_iterations, iterations);

            for (int i = 0; i < 2; i++) {
                uint8_t r, g, b;
                get_color(iterations[i], data->max_iterations, &r, &g, &b);
                /* web/wasm expects rgba */
                data->pixels[y * (data->pitch / sizeof(uint32_t)) + (x + i)] =
                    (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
        }
#endif

        for (; x < data->window_width; x++) {
            complex_t point;
            point.re = data->re_min + (double)x * re_factor;
            point.im = data->im_min + (double)y * im_factor;

            double iterations;
            if (data->mode == RENDER_JULIA)
                iterations = julia_check(point, data->julia_c, data->max_iterations);
            else
                iterations = mandelbrot_check(point, data->max_iterations);

            uint8_t r, g, b;
            get_color(iterations, data->max_iterations, &r, &g, &b);
#if defined(__EMSCRIPTEN__)
            data->pixels[y * (data->pitch / sizeof(uint32_t)) + x] =
                (0xFF << 24) | (b << 16) | (g << 8) | r;
#else
            data->pixels[y * (data->pitch / sizeof(uint32_t)) + x] =
                (0xFF << 24) | (r << 16) | (g << 8) | b;
#endif
        }
    }

    return NULL;
}

static void dispatch_threads(uint32_t* pixels, int pitch, int window_width, int window_height,
                             double re_min, double re_max, double im_min, double im_max,
                             RenderMode mode, complex_t julia_c, int max_iterations) {
    atomic_int next_row = 0;

    for (int i = 0; i < actual_thread_count; i++) {
        thread_data_pool[i] = (thread_data_t){.id = i,
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
                                              .next_row = &next_row};
    }

#if defined(__EMSCRIPTEN__)
    render_thread(&thread_data_pool[0]);
#else
    for (int i = 0; i < actual_thread_count; i++) {
        if (pthread_create(&threads_pool[i], NULL, render_thread, &thread_data_pool[i]) != 0) {
            fprintf(stderr, "failed to spawn thread %d\n", i);
            for (int j = 0; j < i; j++) pthread_join(threads_pool[j], NULL);
            return;
        }
    }

    for (int i = 0; i < actual_thread_count; i++) pthread_join(threads_pool[i], NULL);
#endif
}

void render_mandelbrot_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                                double re_min, double re_max, double im_min, double im_max,
                                int max_iterations) {
    complex_t dummy = {0};
    dispatch_threads(pixels, pitch, window_width, window_height, re_min, re_max, im_min, im_max,
                     RENDER_MANDELBROT, dummy, max_iterations);
}

void render_julia_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                           double re_min, double re_max, double im_min, double im_max,
                           complex_t julia_c, int max_iterations) {
    dispatch_threads(pixels, pitch, window_width, window_height, re_min, re_max, im_min, im_max,
                     RENDER_JULIA, julia_c, max_iterations);
}