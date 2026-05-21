/* test_math.c — unit tests for fractal math kernels.
 *
 * validates scalar, avx2, and 128-bit double-double implementations
 * against known mathematical properties of the mandelbrot and julia sets.
 * run with: make -C tests        (scalar + avx2)
 *           make -C tests test_f128  (includes 128-bit) */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "core_math.h"

/* test runner macros */
#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");

#define EXPECT(cond) if (!(cond)) { \
    fprintf(stderr, "\ntest failed: %s at line %d\n", #cond, __LINE__); \
    exit(1); \
}

/* compare doubles with epsilon tolerance */
static int approx_eq(double a, double b) {
    return fabs(a - b) < 1e-7;
}

void test_mandelbrot_basics(void) {
    TEST_START("mandelbrot_check basics");

    /* point inside the set (origin — main cardioid) */
    complex_t c_inside = {0.0, 0.0};
    EXPECT(mandelbrot_check(c_inside, 100) == 100.0);

    /* point outside the set */
    complex_t c_outside = {2.0, 2.0};
    EXPECT(mandelbrot_check(c_outside, 100) < 100.0);

    /* point that escapes immediately (outside escape radius) */
    complex_t c_immediate = {ESCAPE_RADIUS + 1.0, 0.0};
    EXPECT(mandelbrot_check(c_immediate, 100) < 100.0);

    /* point in period-2 bulb */
    complex_t c_bulb = {-1.0, 0.0};
    EXPECT(mandelbrot_check(c_bulb, 100) == 100.0);

    TEST_END();
}

void test_julia_basics(void) {
    TEST_START("julia_check basics");

    /* z=0.5 with c=0: orbit stays bounded (|z| < 1) */
    complex_t z = {0.5, 0.0};
    complex_t c_julia = {0.0, 0.0};
    EXPECT(julia_check(z, c_julia, 100) == 100.0);

    /* point way outside — escapes immediately */
    complex_t z_out = {2.0, 2.0};
    EXPECT(julia_check(z_out, c_julia, 100) < 100.0);

    TEST_END();
}

void test_smooth_coloring(void) {
    TEST_START("smooth coloring continuity");

    /* verify the smooth coloring formula produces continuous values
     * (not just discrete integers) for points near the boundary */
    complex_t c = {-0.75, 0.1};
    double result = mandelbrot_check(c, 500);

    /* result should be fractional (not an integer) if smooth coloring works */
    double fractional_part = result - floor(result);
    EXPECT(fractional_part > 0.001 && fractional_part < 0.999);

    TEST_END();
}

#ifdef __AVX2__
void test_avx2_consistency(void) {
    TEST_START("avx2 vs scalar consistency");

    /* include points that escape at different stages */
    double re[4] = {0.0, 2.0, 0.2, 11.0};
    double im[4] = {0.0, 2.0, -0.2, 0.0};
    double results_avx[4];
    int max_iters = 100;

    /* test mandelbrot avx */
    mandelbrot_check_avx2(re, im, max_iters, results_avx);
    for (int i = 0; i < 4; i++) {
        complex_t c = {re[i], im[i]};
        double res_scalar = mandelbrot_check(c, max_iters);
        EXPECT(approx_eq(results_avx[i], res_scalar));
    }

    /* test julia avx */
    complex_t c_julia = {-0.8, 0.156};
    julia_check_avx2(re, im, c_julia, max_iters, results_avx);
    for (int i = 0; i < 4; i++) {
        complex_t z = {re[i], im[i]};
        double res_scalar = julia_check(z, c_julia, max_iters);
        EXPECT(approx_eq(results_avx[i], res_scalar));
    }

    TEST_END();
}
#endif

#ifdef USE_SIMD_F128
void test_f128_vs_scalar(void) {
    TEST_START("128-bit vs scalar consistency");

    /* test that mandelbrot_check_f128 produces the same results as scalar.
     * at normal zoom levels the answers must be identical within epsilon. */
    double test_re[] = {0.0, -1.0, 2.0, -0.75, 0.3};
    double test_im[] = {0.0, 0.0, 2.0, 0.1, -0.5};
    int max_iters = 200;

    for (int i = 0; i < 5; i++) {
        complex_t c = {test_re[i], test_im[i]};
        double scalar = mandelbrot_check(c, max_iters);

        simd_f128 cre = simd_f128_from_double(test_re[i]);
        simd_f128 cim = simd_f128_from_double(test_im[i]);
        double f128 = mandelbrot_check_f128(cre, cim, max_iters);

        EXPECT(approx_eq(scalar, f128));
    }

    TEST_END();
}

void test_f128_julia_vs_scalar(void) {
    TEST_START("128-bit julia vs scalar consistency");

    double test_re[] = {0.5, 0.0, -0.3, 2.0};
    double test_im[] = {0.0, 0.5, 0.4, 2.0};
    complex_t c_julia = {-0.8, 0.156};
    int max_iters = 200;

    simd_f128 cre = simd_f128_from_double(c_julia.re);
    simd_f128 cim = simd_f128_from_double(c_julia.im);

    for (int i = 0; i < 4; i++) {
        complex_t z = {test_re[i], test_im[i]};
        double scalar = julia_check(z, c_julia, max_iters);

        simd_f128 zre = simd_f128_from_double(test_re[i]);
        simd_f128 zim = simd_f128_from_double(test_im[i]);
        double f128 = julia_check_f128(zre, zim, cre, cim, max_iters);

        EXPECT(approx_eq(scalar, f128));
    }

    TEST_END();
}

void test_f128_burning_ship_vs_scalar(void) {
    TEST_START("128-bit burning ship vs scalar consistency");

    double test_re[] = {0.0, -1.0, 2.0, -0.45, 0.5};
    double test_im[] = {0.0, 0.0, 2.0, -0.63, 0.5};
    int max_iters = 200;

    for (int i = 0; i < 5; i++) {
        complex_t c = {test_re[i], test_im[i]};
        double scalar = burning_ship_check(c, max_iters);

        simd_f128 cre = simd_f128_from_double(test_re[i]);
        simd_f128 cim = simd_f128_from_double(test_im[i]);
        double f128 = burning_ship_check_f128(cre, cim, max_iters);

        EXPECT(approx_eq(scalar, f128));
    }

    TEST_END();
}
#endif

int main(void) {
    printf("--- starting core math tests ---\n");

    test_mandelbrot_basics();
    test_julia_basics();
    test_smooth_coloring();

#ifdef __AVX2__
    test_avx2_consistency();
#else
    printf("skipped avx2 tests (not compiled with avx2)\n");
#endif

#ifdef USE_SIMD_F128
    test_f128_vs_scalar();
    test_f128_julia_vs_scalar();
    test_f128_burning_ship_vs_scalar();
#else
    printf("skipped 128-bit tests (not compiled with -DUSE_SIMD_F128)\n");
#endif

    printf("--- all tests passed! ---\n");
    return 0;
}
