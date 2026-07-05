/* renderer.c
 *
 * multi-threaded cpu rendering pipeline and worker pool.
 * splits viewport into rows and assigns them dynamically to workers.
 *
 * systems:
 *   - pthread pool lifecycle management (allocation, execution, synchronization)
 *   - dynamic load balancing using row-by-row work distribution
 *   - pixel color conversion and packing into argb8888 framebuffers
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
#include "config_loader.h"

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

struct RendererContext {
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
    int requested_128bit;

    // synchronization
    pthread_mutex_t mutex;
    pthread_mutex_t dispatch_mutex;
    pthread_cond_t work_ready;  // main -> workers: new frame posted
    pthread_cond_t work_done;   // workers -> main: all rows consumed
    int frame_id;               // monotonically increasing, workers compare against last seen
    int threads_idle;           // workers that finished current frame

    // threads list
    pthread_t* threads;
    int actual_thread_count;
    int preset_thread_count;
};

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

static int detect_thread_count(const RendererContext* ctx) {
    if (ctx && ctx->preset_thread_count > 0) return ctx->preset_thread_count;
    int cores = get_cpu_cores();
    int config_threads = get_config_default_thread_count();
    int count = (config_threads > 0) ? config_threads : cores;
    if (count < 1) count = 1;
    if (count > 64) count = 64;
    return count;
}

int get_optimal_thread_count(void) {
    return detect_thread_count(NULL);
}
int get_actual_thread_count(const RendererContext* ctx) {
    return ctx ? ctx->actual_thread_count : 0;
}

// row processing — called from worker threads (and directly on wasm)
static void process_rows(RendererContext* ctx) {
    #define pool (*ctx)
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
                            pool.pixels[y * (pitch_words) + (x + i)] = col;
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
pool.pixels[y * (pitch_words) + x] = col;
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

                        _mm256_storeu_si256((__m256i*)&pool.pixels[y * pitch_words + x], v_colors);
                    } else {
                        for (int i = 0; i < 8; i++) {
                            uint8_t r, g, b;
                            get_color(iterations[i], pool.max_iterations, &r, &g, &b);
pool.pixels[y * pitch_words + (x + i)] =
                                (0xFF << 24) | (b << 16) | (g << 8) | r;
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

                        _mm_storeu_si128((__m128i*)&pool.pixels[y * pitch_words + x], v_colors);
                    } else {
                        for (int i = 0; i < 4; i++) {
                            uint8_t r, g, b;
                            get_color(iterations[i], pool.max_iterations, &r, &g, &b);
pool.pixels[y * pitch_words + (x + i)] =
                                (0xFF << 24) | (b << 16) | (g << 8) | r;
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
pool.pixels[y * pitch_words + (x + i)] = col;
                    }
                }
            }
#elif defined(__ARM_NEON)
            for (; x <= x_end - 2; x += 2) {
                double iterations[2];
                double x_arr[2] = { (double)pool.re_min + (double)x * re_factor, (double)pool.re_min + (double)(x + 1) * re_factor };
                float64x2_t v_re = vld1q_f64(x_arr);
                float64x2_t v_im = vdupq_n_f64((double)pool.im_top + (double)y * im_factor);

                fd->check_neon(v_re, v_im, pool.julia_c, pool.max_iterations, iterations);

                for (int i = 0; i < 2; i++) {
                    if (iterations[i] >= pool.max_iterations || !lut) {
                        pool.pixels[y * pitch_words + (x + i)] = 0xFF000000;
                    } else {
                        int idx = (int)(iterations[i] * 256.0);
                        if (idx < 0) idx = 0;
                        if (idx > max_idx) idx = max_idx;
                        uint32_t col = lut[idx];
pool.pixels[y * pitch_words + (x + i)] = col;
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
pool.pixels[y * pitch_words + x] = col;
                }
            }
        }
    }
    #undef pool
}

#if !defined(__EMSCRIPTEN__)
/* persistent worker thread loop:
 *
 * workers remain alive throughout the application lifecycle to avoid thread creation overhead.
 * they park on a condition variable when idle, and wake up simultaneously via broadcast
 * when a new frame is dispatched.
 */
static void* worker_thread(void* arg) {
    RendererContext* ctx = (RendererContext*)arg;
    #define pool (*ctx)
    int last_frame = 0;

    pthread_mutex_lock(&pool.mutex);
    while (1) {
        /* wait until a new frame is requested or shutdown is initiated.
         * using frame_id comparison protects against spurious wakeups. */
        while (!atomic_load(&pool.shutdown) && pool.frame_id == last_frame)
            pthread_cond_wait(&pool.work_ready, &pool.mutex);

        if (atomic_load(&pool.shutdown)) {
            pthread_mutex_unlock(&pool.mutex);
            return NULL;
        }

        last_frame = pool.frame_id;
        pthread_mutex_unlock(&pool.mutex);

        /* dynamic workload loop (process_rows):
         * workers pull row indices atomically, ensuring automatic load balancing.
         * no mutex is held here to prevent contention on the thread-pool lock. */
        process_rows(ctx);

        pthread_mutex_lock(&pool.mutex);
        /* increment the idle counter. if this was the last thread to finish,
         * signal the main thread that the entire frame is fully rendered. */
        if (++pool.threads_idle == pool.thread_count) pthread_cond_signal(&pool.work_done);
    }
    #undef pool
}
#endif

RendererContext* init_renderer(int max_iterations, int palette_idx) {
    RendererContext* ctx = malloc(sizeof(RendererContext));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(RendererContext));

    #define pool (*ctx)
    pthread_mutex_init(&pool.dispatch_mutex, NULL);

    pthread_mutex_lock(&pool.dispatch_mutex);
    pool.actual_thread_count = detect_thread_count(ctx);
    pool.thread_count = (pool.actual_thread_count > 1) ? pool.actual_thread_count - 1 : 0;
    atomic_store(&pool.shutdown, 0);
    pool.frame_id = 0;
    pool.threads_idle = 0;
    atomic_store(&pool.next_row, 0);
    atomic_store(&pool.next_tile, 0);

    pthread_mutex_init(&pool.mutex, NULL);
    pthread_cond_init(&pool.work_ready, NULL);
    pthread_cond_init(&pool.work_done, NULL);

    pool.threads = malloc(sizeof(pthread_t) * pool.actual_thread_count);
    if (!pool.threads) {
        fprintf(stderr, "fatal: failed to allocate thread pool\n");
        pthread_mutex_unlock(&pool.dispatch_mutex);
        pthread_mutex_destroy(&pool.dispatch_mutex);
        free(ctx);
        return NULL;
    }

#if !defined(__EMSCRIPTEN__)
    for (int i = 0; i < pool.thread_count; i++) {
        if (pthread_create(&pool.threads[i], NULL, worker_thread, ctx) != 0) {
            fprintf(stderr, "fatal: failed to spawn worker thread %d\n", i);
            // shut down and join already-spawned threads safely
            atomic_store(&pool.shutdown, 1);
            pthread_mutex_lock(&pool.mutex);
            pthread_cond_broadcast(&pool.work_ready);
            pthread_mutex_unlock(&pool.mutex);
            for (int j = 0; j < i; j++) {
                pthread_join(pool.threads[j], NULL);
            }
            pthread_mutex_unlock(&pool.dispatch_mutex);
            pthread_mutex_destroy(&pool.dispatch_mutex);
            pthread_mutex_destroy(&pool.mutex);
            pthread_cond_destroy(&pool.work_ready);
            pthread_cond_destroy(&pool.work_done);
            free(pool.threads);
            free(ctx);
            return NULL;
        }
    }
#endif
    init_color_palette(max_iterations, palette_idx);
    pthread_mutex_unlock(&pool.dispatch_mutex);

    #undef pool
    return ctx;
}

void cleanup_renderer(RendererContext* ctx) {
    if (!ctx) return;
    #define pool (*ctx)
    pthread_mutex_lock(&pool.dispatch_mutex);
#if !defined(__EMSCRIPTEN__)
    if (pool.actual_thread_count > 0) {
        pthread_mutex_lock(&pool.mutex);
        atomic_store(&pool.shutdown, 1);
        pthread_cond_broadcast(&pool.work_ready);
        pthread_mutex_unlock(&pool.mutex);

        for (int i = 0; i < pool.thread_count; i++) pthread_join(pool.threads[i], NULL);

        pthread_mutex_destroy(&pool.mutex);
        pthread_cond_destroy(&pool.work_ready);
        pthread_cond_destroy(&pool.work_done);
    }
#endif
    free(pool.threads);
    pthread_mutex_unlock(&pool.dispatch_mutex);
    pthread_mutex_destroy(&pool.dispatch_mutex);
    #undef pool
    free(ctx);
}

int set_renderer_thread_count(RendererContext* ctx, int count) {
    if (!ctx) return 0;
#if !defined(__EMSCRIPTEN__)
    if (count < 1) count = 1;
    if (count > 64) count = 64;

    #define pool (*ctx)
    pthread_mutex_lock(&pool.dispatch_mutex);
    pool.preset_thread_count = count;
    if (pool.actual_thread_count == 0) {
        pthread_mutex_unlock(&pool.dispatch_mutex);
        return 0;
    }
    if (pool.actual_thread_count == count) {
        pthread_mutex_unlock(&pool.dispatch_mutex);
        return 1;
    }

    // stop existing worker threads safely
    pthread_mutex_lock(&pool.mutex);
    atomic_store(&pool.shutdown, 1);
    pthread_cond_broadcast(&pool.work_ready);
    pthread_mutex_unlock(&pool.mutex);

    for (int i = 0; i < pool.thread_count; i++) {
        pthread_join(pool.threads[i], NULL);
    }
    free(pool.threads);
    pool.threads = NULL;

    pool.actual_thread_count = count;
    pool.thread_count = (pool.actual_thread_count > 1) ? pool.actual_thread_count - 1 : 0;
    atomic_store(&pool.shutdown, 0);
    pool.threads_idle = 0;
    atomic_store(&pool.next_row, 0);
    atomic_store(&pool.next_tile, 0);

    pool.threads = malloc(sizeof(pthread_t) * pool.actual_thread_count);
    if (!pool.threads) {
        fprintf(stderr, "error: failed to allocate thread pool during resize\n");
        pthread_mutex_unlock(&pool.dispatch_mutex);
        return 0;
    }
    for (int i = 0; i < pool.thread_count; i++) {
        if (pthread_create(&pool.threads[i], NULL, worker_thread, ctx) != 0) {
            fprintf(stderr, "error: failed to spawn worker thread %d during resize\n", i);
            // kill already-spawned threads before returning to avoid a half-initialized pool
            atomic_store(&pool.shutdown, 1);
            pthread_mutex_lock(&pool.mutex);
            pthread_cond_broadcast(&pool.work_ready);
            pthread_mutex_unlock(&pool.mutex);
            for (int j = 0; j < i; j++) pthread_join(pool.threads[j], NULL);
            pthread_mutex_unlock(&pool.dispatch_mutex);
            return 0;
        }
    }
    printf("thread pool dynamically resized to %d threads.\n", pool.actual_thread_count);
    pthread_mutex_unlock(&pool.dispatch_mutex);
    #undef pool
    return 1;
#else
    (void)ctx;
    (void)count;
    return 1;
#endif
}

// dispatch a render job — returns only after all rows are painted
void render_fractal_threaded(RendererContext* ctx, const RenderJob* job) {
    if (!ctx || !job) return;
    #define pool (*ctx)
    pthread_mutex_lock(&pool.dispatch_mutex);
    pthread_mutex_lock(&pool.mutex);
    pool.pixels = job->pixels;
    pool.pitch = job->pitch;
    pool.window_width = job->window_width;
    pool.window_height = job->window_height;
    pool.re_min = job->re_min;
    pool.re_max = job->re_max;
    pool.im_top = job->im_top;
    pool.im_bottom = job->im_bottom;
    pool.mode = job->mode;
    pool.julia_c = job->julia_c;
    pool.max_iterations = job->max_iterations;
    pool.use_128bit = pool.requested_128bit;

    int cols_tiles = (job->window_width + TILE_SIZE - 1) / TILE_SIZE;
    int rows_tiles = (job->window_height + TILE_SIZE - 1) / TILE_SIZE;
    pool.cols_tiles = cols_tiles;
    pool.total_tiles = cols_tiles * rows_tiles;
    atomic_store(&pool.next_tile, 0);
    atomic_store(&pool.next_row, 0);

#if defined(__EMSCRIPTEN__)
    pthread_mutex_unlock(&pool.mutex);
    process_rows(ctx);
#else
    pool.threads_idle = 0;
    pool.frame_id++;
    pthread_cond_broadcast(&pool.work_ready);
    pthread_mutex_unlock(&pool.mutex);  // release lock to let workers start

    /* let main thread participate in rendering instead of just idling.
     * note: main thread doesn't increment pool.threads_idle, so thread_count
     * is exactly the number of workers we need to wait for. */
    process_rows(ctx);

    pthread_mutex_lock(&pool.mutex);
    while (pool.threads_idle < pool.thread_count) pthread_cond_wait(&pool.work_done, &pool.mutex);
    pthread_mutex_unlock(&pool.mutex);
#endif
    pthread_mutex_unlock(&pool.dispatch_mutex);
    #undef pool
}

// dynamic precision control
void set_cpu_precision(RendererContext* ctx, int use_128bit) {
    if (ctx) {
        ctx->requested_128bit = use_128bit;
    }
}

int get_cpu_precision(const RendererContext* ctx) {
    return ctx ? ctx->requested_128bit : 0;
}
