/* benchmark_math.c
 *
 * performance benchmarking for mandelbrot math kernels.
 * measures and compares scalar vs vectorized simd execution speeds.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core_math.h"

#define MAX_ITERATIONS 1000
#define GRID_WIDTH 1024
#define GRID_HEIGHT 1024

// returns time in seconds
double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    printf("==========================================\n");
    printf(" Mandelbrot Math Kernel Benchmarks        \n");
    printf(" Resolution: %d x %d (%.2f Million Pixels)\n", GRID_WIDTH, GRID_HEIGHT,
           (GRID_WIDTH * GRID_HEIGHT) / 1e6);
    printf(" Max Iterations: %d                       \n", MAX_ITERATIONS);
    printf("==========================================\n\n");

    const double x_min = -2.0;
    const double x_max = 1.0;
    const double y_min = -1.5;
    const double y_max = 1.5;
    const double dx = (x_max - x_min) / GRID_WIDTH;
    const double dy = (y_max - y_min) / GRID_HEIGHT;

    double* results = malloc(GRID_WIDTH * GRID_HEIGHT * sizeof(double));
    if (!results) {
        printf("error: memory allocation failed\n");
        return 1;
    }

    double start, end, time_taken;
    long long total_pixels = GRID_WIDTH * GRID_HEIGHT;

    // scalar double benchmark
    printf("1. Running Scalar (double)...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            complex_t c = {x_min + x * dx, y_min + y * dy};
            results[y * GRID_WIDTH + x] = mandelbrot_check(c, MAX_ITERATIONS);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);

    // scalar double benchmark - julia
    printf("1a. Running Scalar (double) - Julia...\n");
    start = get_time_sec();
    complex_t julia_c = {-0.7, 0.27015};
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            complex_t z = {x_min + x * dx, y_min + y * dy};
            results[y * GRID_WIDTH + x] = julia_check(z, julia_c, MAX_ITERATIONS);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);

    // scalar double benchmark - tricorn
    printf("1b. Running Scalar (double) - Tricorn...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            complex_t c = {x_min + x * dx, y_min + y * dy};
            results[y * GRID_WIDTH + x] = tricorn_check(c, MAX_ITERATIONS);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);

    // scalar double benchmark - celtic
    printf("1c. Running Scalar (double) - Celtic...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            complex_t c = {x_min + x * dx, y_min + y * dy};
            results[y * GRID_WIDTH + x] = celtic_check(c, MAX_ITERATIONS);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);

#ifdef __AVX2__
    // avx2 double benchmark
    printf("2. Running AVX2 (4x double)...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x += 4) {
            __m256d cre = _mm256_set_pd(x_min + (x + 3) * dx, x_min + (x + 2) * dx,
                                        x_min + (x + 1) * dx, x_min + x * dx);
            __m256d cim = _mm256_set1_pd(y_min + y * dy);
            mandelbrot_check_avx2(cre, cim, MAX_ITERATIONS, &results[y * GRID_WIDTH + x]);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);
#endif

#ifdef __AVX512F__
    // avx512 double benchmark
    printf("2b. Running AVX512 (8x double)...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x += 8) {
            __m512d cre =
                _mm512_set_pd(x_min + (x + 7) * dx, x_min + (x + 6) * dx, x_min + (x + 5) * dx,
                              x_min + (x + 4) * dx, x_min + (x + 3) * dx, x_min + (x + 2) * dx,
                              x_min + (x + 1) * dx, x_min + x * dx);
            __m512d cim = _mm512_set1_pd(y_min + y * dy);
            mandelbrot_check_avx512(cre, cim, MAX_ITERATIONS, &results[y * GRID_WIDTH + x]);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);
#endif

#ifdef USE_SIMD_F128
    // scalar f128 benchmark
    printf("3. Running simd-f128 (128-bit double-double)...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            simd_f128 cre = simd_f128_from_double(x_min + x * dx);
            simd_f128 cim = simd_f128_from_double(y_min + y * dy);
            results[y * GRID_WIDTH + x] = mandelbrot_check_f128(cre, cim, MAX_ITERATIONS);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);

#ifdef __AVX2__
    // avx2 f128 benchmark
    printf("4. Running simd-f128x4 (AVX2 128-bit double-double)...\n");
    start = get_time_sec();
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x += 4) {
            simd_f128x4 cre = simd_f128x4_from_doubles(x_min + x * dx, x_min + (x + 1) * dx,
                                                       x_min + (x + 2) * dx, x_min + (x + 3) * dx);
            simd_f128x4 cim = simd_f128x4_from_doubles(y_min + y * dy, y_min + y * dy,
                                                       y_min + y * dy, y_min + y * dy);

            mandelbrot_check_f128x4(cre, cim, MAX_ITERATIONS, &results[y * GRID_WIDTH + x]);
        }
    }
    end = get_time_sec();
    time_taken = end - start;
    printf("   -> Time taken: %.4f seconds\n", time_taken);
    printf("   -> Speed:      %.2f Mpx/s\n\n", (total_pixels / 1e6) / time_taken);
#endif
#endif

    free(results);
    printf("Benchmark complete.\n");
    return 0;
}
