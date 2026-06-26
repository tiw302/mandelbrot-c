/* renderer.c
 *
 * handles multi-threaded and vectorized CPU rendering of fractal sets.
 * orchestrates the thread pool, tile-based parallel rendering, and pixel formats.
 */

#include "renderer.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "core_math.h"
#include "ini_config.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/threading.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__) || defined(__MACH__) || defined(__linux__) || defined(__FreeBSD__)
#include <unistd.h>
#endif

/* persistent thread pool
 *
 * threads are spawned once at init_renderer() and park on a condition
 * variable between frames. the main thread writes the job descriptor,
 * increments frame_id, then broadcasts work_ready. workers race to claim
 * rows via an atomic counter. when a worker finishes all rows it increments
 * threads_idle and signals work_done. the main thread waits on work_done
 * until threads_idle == thread_count before returning from dispatch().
 *
 * this eliminates pthread_create/pthread_join overhead on every render call,
 * which is significant when iteration counts are low (fast renders).
 */

#define TILE_SIZE 32

typedef struct {
    // job descriptor — written by main thread before each frame
    uint32_t* pixels;
    int pitch;
    int window_width;
    int window_height;
    precise_float re_min, re_max;
    precise_float im_top, im_bottom;
    RenderMode mode;
    complex_t julia_c;
    int max_iterations;

    // shared atomic tile and row counter — threads race to claim the next item
    atomic_int next_row;
    atomic_int next_tile;
    int cols_tiles;
    int total_tiles;

    // pool metadata
    int thread_count;
    // thread pool shutdown flag. using stdatomic ensures cross-core visibility.
    atomic_int shutdown;
    int use_128bit;

    // synchronization
    pthread_mutex_t mutex;
    pthread_cond_t work_ready;  // main -> workers: new frame posted
    pthread_cond_t work_done;   // workers -> main: all rows consumed
    int frame_id;               // monotonically increasing, workers compare against last seen
    int threads_idle;           // workers that finished current frame
} RenderPool;

static RenderPool pool = {0};
static pthread_t* threads_pool = NULL;
static int actual_thread_count = 0;
static int preset_thread_count = 0;
static int requested_128bit = 0;

// cpu core detection
static int get_cpu_cores(void) {
#if defined(__EMSCRIPTEN__)
    return 1;
#elif defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}

static int detect_thread_count(void) {
    if (preset_thread_count > 0) return preset_thread_count;
    int cores = get_cpu_cores();
    int config_threads = get_config_default_thread_count();
    int count = (config_threads > 0) ? config_threads : cores;
    if (count < 1) count = 1;
    if (count > 64) count = 64;
    return count;
}

int get_optimal_thread_count(void) {
    return detect_thread_count();
}
int get_actual_thread_count(void) {
    return actual_thread_count;
}

// row processing — called from worker threads (and directly on wasm)
static void process_rows(void) {
    const FractalDefinition* fd = get_fractal_by_mode(pool.mode);
    if (!fd) return;

    uint32_t* lut = get_palette_lut();
    int max_idx = get_palette_lut_size() - 1;

#ifdef USE_SIMD_F128
    if (pool.use_128bit) {
        simd_f128 re_min = simd_f128_from_hi_lo(
            (double)pool.re_min, (double)(pool.re_min - (precise_float)(double)pool.re_min));
        simd_f128 im_min = simd_f128_from_hi_lo(
            (double)pool.im_top, (double)(pool.im_top - (precise_float)(double)pool.im_top));

        precise_float pref =
            (pool.window_width > 0) ? (pool.re_max - pool.re_min) / pool.window_width : 0.0;
        simd_f128 re_factor =
            simd_f128_from_hi_lo((double)pref, (double)(pref - (precise_float)(double)pref));

        precise_float pimf =
            (pool.window_height > 0) ? (pool.im_bottom - pool.im_top) / pool.window_height : 0.0;
        simd_f128 im_factor =
            simd_f128_from_hi_lo((double)pimf, (double)(pimf - (precise_float)(double)pimf));

        simd_f128 julia_cre = simd_f128_from_double(pool.julia_c.re);
        simd_f128 julia_cim = simd_f128_from_double(pool.julia_c.im);

        int tile_idx;
        int cols_tiles = pool.cols_tiles;
        int total_tiles = pool.total_tiles;
        int pitch_words = pool.pitch / sizeof(uint32_t);
        while ((tile_idx = atomic_fetch_add(&pool.next_tile, 1)) < total_tiles) {
            if (atomic_load(&pool.shutdown)) break;
            int tile_x = (tile_idx % cols_tiles) * TILE_SIZE;
            int tile_y = (tile_idx / cols_tiles) * TILE_SIZE;
            int y_end = tile_y + TILE_SIZE;
            if (y_end > pool.window_height) y_end = pool.window_height;
            int x_end = tile_x + TILE_SIZE;
            if (x_end > pool.window_width) x_end = pool.window_width;

            for (int y = tile_y; y < y_end; y++) {
                simd_f128 y_128 = simd_f128_from_double((double)y);
                simd_f128 y_im = simd_f128_add(im_min, simd_f128_mul(y_128, im_factor));

                int x = tile_x;
#ifdef __AVX2__
                double re_min_hi, re_min_lo;
                double re_fac_hi, re_fac_lo;
                double y_im_hi, y_im_lo;
                double julia_cre_hi, julia_cre_lo;
                double julia_cim_hi, julia_cim_lo;

                simd_f128_extract(re_min, &re_min_hi, &re_min_lo);
                simd_f128_extract(re_factor, &re_fac_hi, &re_fac_lo);
                simd_f128_extract(y_im, &y_im_hi, &y_im_lo);
                simd_f128_extract(julia_cre, &julia_cre_hi, &julia_cre_lo);
                simd_f128_extract(julia_cim, &julia_cim_hi, &julia_cim_lo);

                simd_f128x4 v_re_min = {_mm256_set1_pd(re_min_hi), _mm256_set1_pd(re_min_lo)};
                simd_f128x4 v_re_fac = {_mm256_set1_pd(re_fac_hi), _mm256_set1_pd(re_fac_lo)};
                simd_f128x4 v_im_val = {_mm256_set1_pd(y_im_hi), _mm256_set1_pd(y_im_lo)};
                simd_f128x4 v_julia_cre = {_mm256_set1_pd(julia_cre_hi),
                                           _mm256_set1_pd(julia_cre_lo)};
                simd_f128x4 v_julia_cim = {_mm256_set1_pd(julia_cim_hi),
                                           _mm256_set1_pd(julia_cim_lo)};

                for (; x <= x_end - 4; x += 4) {
                    double iterations[4];
                    simd_f128x4 v_x = simd_f128x4_from_doubles((double)x, (double)(x + 1),
                                                               (double)(x + 2), (double)(x + 3));
                    simd_f128x4 v_re = simd_f128x4_add(v_re_min, simd_f128x4_mul(v_x, v_re_fac));

                    fd->check_avx2_f128(v_re, v_im_val, v_julia_cre, v_julia_cim,
                                        pool.max_iterations, iterations);

                    for (int i = 0; i < 4; i++) {
                        if (iterations[i] >= pool.max_iterations || !lut) {
                            pool.pixels[y * (pitch_words) + (x + i)] = 0xFF000000;
                        } else {
                            int idx = (int)(iterations[i] * 256.0);
                            if (idx < 0) idx = 0;
                            if (idx > max_idx) idx = max_idx;
                            uint32_t col = lut[idx];
#if defined(__EMSCRIPTEN__)
                            uint32_t r = (col >> 16) & 0xFF;
                            uint32_t b = col & 0xFF;
                            pool.pixels[y * (pitch_words) + (x + i)] =
                                (col & 0xFF00FF00) | (b << 16) | r;
#else
                            pool.pixels[y * (pitch_words) + (x + i)] = col;
#endif
                        }
                    }
                }
#endif

                // scalar tail for 128bit
                for (; x < x_end; x++) {
                    simd_f128 x_128 = simd_f128_from_double((double)x);
                    simd_f128 x_re = simd_f128_add(re_min, simd_f128_mul(x_128, re_factor));

                    double iterations = fd->check_scalar_f128(x_re, y_im, julia_cre, julia_cim,
                                                              pool.max_iterations);

                    if (iterations >= pool.max_iterations || !lut) {
                        pool.pixels[y * (pitch_words) + x] = 0xFF000000;
                    } else {
                        int idx = (int)(iterations * 256.0);
                        if (idx < 0) idx = 0;
                        if (idx > max_idx) idx = max_idx;
                        uint32_t col = lut[idx];
#if defined(__EMSCRIPTEN__)
                        uint32_t r = (col >> 16) & 0xFF;
                        uint32_t b = col & 0xFF;
                        pool.pixels[y * (pitch_words) + x] = (col & 0xFF00FF00) | (b << 16) | r;
#else
                        pool.pixels[y * (pitch_words) + x] = col;
#endif
                    }
                }
            }
        }
        return;
    }
#endif

    double re_factor =
        (pool.window_width > 0) ? (double)((pool.re_max - pool.re_min) / pool.window_width) : 0.0;
    double im_factor = (pool.window_height > 0)
                           ? (double)((pool.im_bottom - pool.im_top) / pool.window_height)
                           : 0.0;

    int tile_idx;
    int cols_tiles = pool.cols_tiles;
    int total_tiles = pool.total_tiles;
    int pitch_words = pool.pitch / sizeof(uint32_t);
    while ((tile_idx = atomic_fetch_add(&pool.next_tile, 1)) < total_tiles) {
        if (atomic_load(&pool.shutdown)) break;
        int tile_x = (tile_idx % cols_tiles) * TILE_SIZE;
        int tile_y = (tile_idx / cols_tiles) * TILE_SIZE;
        int y_end = tile_y + TILE_SIZE;
        if (y_end > pool.window_height) y_end = pool.window_height;
        int x_end = tile_x + TILE_SIZE;
        if (x_end > pool.window_width) x_end = pool.window_width;

        for (int y = tile_y; y < y_end; y++) {
            int x = tile_x;

#if defined(__AVX512F__)
            {
                __m512d v_re_min = _mm512_set1_pd((double)pool.re_min);
                __m512d v_re_fac = _mm512_set1_pd(re_factor);
                __m512d v_im_val = _mm512_set1_pd((double)pool.im_top + (double)y * im_factor);
                __m512d v_offsets = _mm512_set_pd(7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0);

                __m256i v_zero = _mm256_setzero_si256();
                __m256i v_max_idx = _mm256_set1_epi32(max_idx);
                __m512d v_mult = _mm512_set1_pd(256.0);

                for (; x <= x_end - 8; x += 8) {
                    double iterations[8];
                    __m512d v_x = _mm512_add_pd(_mm512_set1_pd((double)x), v_offsets);
                    __m512d v_re = _mm512_add_pd(v_re_min, _mm512_mul_pd(v_x, v_re_fac));

                    fd->check_avx512(v_re, v_im_val, pool.julia_c, pool.max_iterations, iterations);

                    if (lut) {
                        __m512d v_iters = _mm512_loadu_pd(iterations);
                        __m512d v_idx_pd = _mm512_mul_pd(v_iters, v_mult);
                        __m256i v_idx = _mm512_cvttpd_epi32(v_idx_pd);
                        v_idx = _mm256_max_epi32(v_zero, v_idx);
                        v_idx = _mm256_min_epi32(v_idx, v_max_idx);
                        __m256i v_colors = _mm256_i32gather_epi32((const int*)lut, v_idx, 4);

                        __m256i v_iters_int = _mm512_cvttpd_epi32(v_iters);
                        __m256i v_max_iters_int = _mm256_set1_epi32(pool.max_iterations);
                        __m256i v_mask = _mm256_cmpgt_epi32(v_max_iters_int, v_iters_int);
                        v_colors = _mm256_and_si256(v_colors, v_mask);

#if defined(__EMSCRIPTEN__)
                        __m256i r = _mm256_and_si256(_mm256_srli_epi32(v_colors, 16),
                                                     _mm256_set1_epi32(0xFF));
                        __m256i b = _mm256_and_si256(v_colors, _mm256_set1_epi32(0xFF));
                        __m256i g_alpha = _mm256_and_si256(v_colors, _mm256_set1_epi32(0xFF00FF00));
                        v_colors =
                            _mm256_or_si256(_mm256_or_si256(g_alpha, _mm256_slli_epi32(b, 16)), r);
#endif
                        _mm256_storeu_si256((__m256i*)&pool.pixels[y * pitch_words + x], v_colors);
                    } else {
                        for (int i = 0; i < 8; i++) {
                            uint8_t r, g, b;
                            get_color(iterations[i], pool.max_iterations, &r, &g, &b);
#if defined(__EMSCRIPTEN__)
                            pool.pixels[y * pitch_words + (x + i)] =
                                (0xFF << 24) | (b << 16) | (g << 8) | r;
#else
                            pool.pixels[y * pitch_words + (x + i)] =
                                (0xFF << 24) | (r << 16) | (g << 8) | b;
#endif
                        }
                    }
                }
            }
#elif defined(__AVX2__)
            {
                __m256d v_re_min = _mm256_set1_pd((double)pool.re_min);
                __m256d v_re_fac = _mm256_set1_pd(re_factor);
                __m256d v_im_val = _mm256_set1_pd((double)pool.im_top + (double)y * im_factor);
                __m256d v_offsets = _mm256_set_pd(3.0, 2.0, 1.0, 0.0);

                __m128i v_zero = _mm_setzero_si128();
                __m128i v_max_idx = _mm_set1_epi32(max_idx);
                __m256d v_mult = _mm256_set1_pd(256.0);

                for (; x <= x_end - 4; x += 4) {
                    double iterations[4];
                    __m256d v_x = _mm256_add_pd(_mm256_set1_pd((double)x), v_offsets);
                    __m256d v_re = _mm256_add_pd(v_re_min, _mm256_mul_pd(v_x, v_re_fac));

                    fd->check_avx2(v_re, v_im_val, pool.julia_c, pool.max_iterations, iterations);

                    if (lut) {
                        __m256d v_iters = _mm256_loadu_pd(iterations);
                        __m256d v_idx_pd = _mm256_mul_pd(v_iters, v_mult);
                        __m128i v_idx = _mm256_cvttpd_epi32(v_idx_pd);
                        v_idx = _mm_max_epi32(v_zero, v_idx);
                        v_idx = _mm_min_epi32(v_idx, v_max_idx);
                        __m128i v_colors = _mm_i32gather_epi32((const int*)lut, v_idx, 4);

                        __m128i v_iters_int = _mm256_cvttpd_epi32(v_iters);
                        __m128i v_max_iters_int = _mm_set1_epi32(pool.max_iterations);
                        __m128i v_mask = _mm_cmpgt_epi32(v_max_iters_int, v_iters_int);
                        v_colors = _mm_and_si128(v_colors, v_mask);

#if defined(__EMSCRIPTEN__)
                        __m128i r =
                            _mm_and_si128(_mm_srli_epi32(v_colors, 16), _mm_set1_epi32(0xFF));
                        __m128i b = _mm_and_si128(v_colors, _mm_set1_epi32(0xFF));
                        __m128i g_alpha = _mm_and_si128(v_colors, _mm_set1_epi32(0xFF00FF00));
                        v_colors = _mm_or_si128(_mm_or_si128(g_alpha, _mm_slli_epi32(b, 16)), r);
#endif
                        _mm_storeu_si128((__m128i*)&pool.pixels[y * pitch_words + x], v_colors);
                    } else {
                        for (int i = 0; i < 4; i++) {
                            uint8_t r, g, b;
                            get_color(iterations[i], pool.max_iterations, &r, &g, &b);
#if defined(__EMSCRIPTEN__)
                            pool.pixels[y * pitch_words + (x + i)] =
                                (0xFF << 24) | (b << 16) | (g << 8) | r;
#else
                            pool.pixels[y * pitch_words + (x + i)] =
                                (0xFF << 24) | (r << 16) | (g << 8) | b;
#endif
                        }
                    }
                }
            }
#elif defined(__wasm_simd128__)
            for (; x <= x_end - 2; x += 2) {
                double iterations[2];
                v128_t v_re = wasm_f64x2_make((double)pool.re_min + (double)x * re_factor,
                                              (double)pool.re_min + (double)(x + 1) * re_factor);
                v128_t v_im = wasm_f64x2_splat((double)pool.im_top + (double)y * im_factor);

                fd->check_wasm_simd128(v_re, v_im, pool.julia_c, pool.max_iterations, iterations);

                for (int i = 0; i < 2; i++) {
                    if (iterations[i] >= pool.max_iterations || !lut) {
                        pool.pixels[y * pitch_words + (x + i)] = 0xFF000000;
                    } else {
                        int idx = (int)(iterations[i] * 256.0);
                        if (idx < 0) idx = 0;
                        if (idx > max_idx) idx = max_idx;
                        uint32_t col = lut[idx];
#if defined(__EMSCRIPTEN__)
                        uint32_t r = (col >> 16) & 0xFF;
                        uint32_t b = col & 0xFF;
                        pool.pixels[y * pitch_words + (x + i)] = (col & 0xFF00FF00) | (b << 16) | r;
#else
                        pool.pixels[y * pitch_words + (x + i)] = col;
#endif
                    }
                }
            }
#endif

            // scalar tail
            for (; x < x_end; x++) {
                complex_t point = {(double)pool.re_min + (double)x * re_factor,
                                   (double)pool.im_top + (double)y * im_factor};
                double iterations = fd->check_scalar(point, pool.julia_c, pool.max_iterations);

                if (iterations >= pool.max_iterations || !lut) {
                    pool.pixels[y * pitch_words + x] = 0xFF000000;
                } else {
                    int idx = (int)(iterations * 256.0);
                    if (idx < 0) idx = 0;
                    if (idx > max_idx) idx = max_idx;
                    uint32_t col = lut[idx];
#if defined(__EMSCRIPTEN__)
                    uint32_t r = (col >> 16) & 0xFF;
                    uint32_t b = col & 0xFF;
                    pool.pixels[y * pitch_words + x] = (col & 0xFF00FF00) | (b << 16) | r;
#else
                    pool.pixels[y * pitch_words + x] = col;
#endif
                }
            }
        }
    }
}

#if !defined(__EMSCRIPTEN__)
// persistent worker thread — parks between frames, wakes on broadcast
static void* worker_thread(void* arg) {
    (void)arg;
    int last_frame = 0;

    pthread_mutex_lock(&pool.mutex);
    while (1) {
        while (!atomic_load(&pool.shutdown) && pool.frame_id == last_frame)
            pthread_cond_wait(&pool.work_ready, &pool.mutex);

        if (atomic_load(&pool.shutdown)) {
            pthread_mutex_unlock(&pool.mutex);
            return NULL;
        }

        last_frame = pool.frame_id;
        pthread_mutex_unlock(&pool.mutex);

        process_rows();  // hot loop — no lock held

        pthread_mutex_lock(&pool.mutex);
        if (++pool.threads_idle == pool.thread_count) pthread_cond_signal(&pool.work_done);
    }
}
#endif

void init_renderer(int max_iterations, int palette_idx) {
    if (actual_thread_count == 0) {
        actual_thread_count = detect_thread_count();
        pool.thread_count = (actual_thread_count > 1) ? actual_thread_count - 1 : 0;
        atomic_store(&pool.shutdown, 0);
        pool.frame_id = 0;
        pool.threads_idle = 0;
        atomic_store(&pool.next_row, 0);
        atomic_store(&pool.next_tile, 0);

        pthread_mutex_init(&pool.mutex, NULL);
        pthread_cond_init(&pool.work_ready, NULL);
        pthread_cond_init(&pool.work_done, NULL);

        threads_pool = malloc(sizeof(pthread_t) * actual_thread_count);
        if (!threads_pool) {
            fprintf(stderr, "fatal: failed to allocate thread pool\n");
            exit(1);
        }

#if !defined(__EMSCRIPTEN__)
        for (int i = 0; i < pool.thread_count; i++) {
            if (pthread_create(&threads_pool[i], NULL, worker_thread, NULL) != 0) {
                fprintf(stderr, "fatal: failed to spawn worker thread %d\n", i);
                exit(1);
            }
        }
#endif
    }
    init_color_palette(max_iterations, palette_idx);
}

void cleanup_renderer(void) {
#if !defined(__EMSCRIPTEN__)
    if (actual_thread_count > 0) {
        pthread_mutex_lock(&pool.mutex);
        atomic_store(&pool.shutdown, 1);
        pthread_cond_broadcast(&pool.work_ready);
        pthread_mutex_unlock(&pool.mutex);

        for (int i = 0; i < pool.thread_count; i++) pthread_join(threads_pool[i], NULL);

        pthread_mutex_destroy(&pool.mutex);
        pthread_cond_destroy(&pool.work_ready);
        pthread_cond_destroy(&pool.work_done);
    }
#endif
    free(threads_pool);
    threads_pool = NULL;
    actual_thread_count = 0;
}

void set_renderer_thread_count(int count) {
#if !defined(__EMSCRIPTEN__)
    if (count < 1) count = 1;
    if (count > 64) count = 64;

    preset_thread_count = count;
    if (actual_thread_count == 0) return;
    if (actual_thread_count == count) return;

    // stop existing worker threads safely
    pthread_mutex_lock(&pool.mutex);
    atomic_store(&pool.shutdown, 1);
    pthread_cond_broadcast(&pool.work_ready);
    pthread_mutex_unlock(&pool.mutex);

    for (int i = 0; i < pool.thread_count; i++) {
        pthread_join(threads_pool[i], NULL);
    }
    free(threads_pool);
    threads_pool = NULL;

    actual_thread_count = count;
    pool.thread_count = (actual_thread_count > 1) ? actual_thread_count - 1 : 0;
    atomic_store(&pool.shutdown, 0);
    pool.threads_idle = 0;
    atomic_store(&pool.next_row, 0);
    atomic_store(&pool.next_tile, 0);

    threads_pool = malloc(sizeof(pthread_t) * actual_thread_count);
    if (!threads_pool) {
        fprintf(stderr, "fatal: failed to allocate thread pool\n");
        exit(1);
    }

    for (int i = 0; i < pool.thread_count; i++) {
        if (pthread_create(&threads_pool[i], NULL, worker_thread, NULL) != 0) {
            fprintf(stderr, "fatal: failed to spawn worker thread %d\n", i);
            exit(1);
        }
    }
    printf("thread pool dynamically resized to %d threads.\n", actual_thread_count);
#else
    (void)count;
#endif
}

// dispatch a render job — returns only after all rows are painted
static void dispatch(uint32_t* pixels, int pitch, int window_width, int window_height,
                     precise_float re_min, precise_float re_max, precise_float im_top,
                     precise_float im_bottom, RenderMode mode, complex_t julia_c,
                     int max_iterations) {
    pthread_mutex_lock(&pool.mutex);
    pool.pixels = pixels;
    pool.pitch = pitch;
    pool.window_width = window_width;
    pool.window_height = window_height;
    pool.re_min = re_min;
    pool.re_max = re_max;
    pool.im_top = im_top;
    pool.im_bottom = im_bottom;
    pool.mode = mode;
    pool.julia_c = julia_c;
    pool.max_iterations = max_iterations;
    pool.use_128bit = requested_128bit;

    int cols_tiles = (window_width + TILE_SIZE - 1) / TILE_SIZE;
    int rows_tiles = (window_height + TILE_SIZE - 1) / TILE_SIZE;
    pool.cols_tiles = cols_tiles;
    pool.total_tiles = cols_tiles * rows_tiles;
    atomic_store(&pool.next_tile, 0);
    atomic_store(&pool.next_row, 0);

#if defined(__EMSCRIPTEN__)
    pthread_mutex_unlock(&pool.mutex);
    process_rows();
#else
    pool.threads_idle = 0;
    pool.frame_id++;
    pthread_cond_broadcast(&pool.work_ready);
    pthread_mutex_unlock(&pool.mutex);  // release lock to let workers start

    /* let main thread participate in rendering instead of just idling.
     * note: main thread doesn't increment pool.threads_idle, so thread_count
     * is exactly the number of workers we need to wait for. */
    process_rows();

    pthread_mutex_lock(&pool.mutex);
    while (pool.threads_idle < pool.thread_count) pthread_cond_wait(&pool.work_done, &pool.mutex);
    pthread_mutex_unlock(&pool.mutex);
#endif
}

void render_mandelbrot_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                                precise_float re_min, precise_float re_max, precise_float im_top,
                                precise_float im_bottom, int max_iterations) {
    complex_t dummy = {0};
    dispatch(pixels, pitch, window_width, window_height, re_min, re_max, im_top, im_bottom,
             RENDER_MANDELBROT, dummy, max_iterations);
}

void render_julia_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                           precise_float re_min, precise_float re_max, precise_float im_top,
                           precise_float im_bottom, complex_t julia_c, int max_iterations) {
    dispatch(pixels, pitch, window_width, window_height, re_min, re_max, im_top, im_bottom,
             RENDER_JULIA, julia_c, max_iterations);
}

void render_burning_ship_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                                  precise_float re_min, precise_float re_max, precise_float im_top,
                                  precise_float im_bottom, int max_iterations) {
    complex_t dummy = {0};
    dispatch(pixels, pitch, window_width, window_height, re_min, re_max, im_top, im_bottom,
             RENDER_BURNING_SHIP, dummy, max_iterations);
}

// legacy symbol — kept so existing call sites in main files compile unchanged
void* render_thread(void* arg) {
    (void)arg;
    process_rows();
    return NULL;
}

// dynamic precision control
void set_cpu_precision(int use_128bit) {
    requested_128bit = use_128bit;
}

int get_cpu_precision(void) {
    return requested_128bit;
}
