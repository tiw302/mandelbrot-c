/* test_simd_math.c
 *
 * unit tests for the simd math operations.
 * validates correctness of vector instructions for Mandelbrot calculations.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_SIMD_F128
#define SIMD_F128_IMPLEMENTATION
#include "core_math.h"

void assert_approx_equal(double actual, double expected, const char* name, double tol) {
    if (isnan(actual) && isnan(expected)) {
        printf("PASS: %s - Got NaN\n", name);
        return;
    }
    if (isinf(actual) && isinf(expected) && ((actual > 0) == (expected > 0))) {
        printf("PASS: %s - Got Inf\n", name);
        return;
    }
    if (isnan(actual) || isnan(expected)) {
        printf("FAIL: %s - Expected: %g, Got: %g\n", name, expected, actual);
        exit(1);
    }
    if (fabs(actual - expected) > tol) {
        printf("FAIL: %s - Expected: %g, Got: %g\n", name, expected, actual);
        exit(1);
    }
    printf("PASS: %s - Got %g\n", name, actual);
}

void extract_simd_f128x4(simd_f128x4 vec, double out_hi[4], double out_lo[4]) {
    _mm256_storeu_pd(out_hi, vec.hi);
    _mm256_storeu_pd(out_lo, vec.lo);
}

/*
 * [TEST CASE] simd division
 * tests the functionality of simd double-single division.
 */
void test_div() {
    printf("Testing simd_f128x4_div...\n");
    simd_f128x4 a, b, res;
    double ahi[4] = {10.0, 1.0, 0.0, INFINITY};
    double alo[4] = {0.0, 0.0, 0.0, 0.0};
    double bhi[4] = {2.0, 0.0, 0.0, INFINITY};
    double blo[4] = {0.0, 0.0, 0.0, 0.0};

    a.hi = _mm256_loadu_pd(ahi);
    a.lo = _mm256_loadu_pd(alo);
    b.hi = _mm256_loadu_pd(bhi);
    b.lo = _mm256_loadu_pd(blo);

    res = simd_f128x4_div(a, b);

    double rhi[4], rlo[4];
    extract_simd_f128x4(res, rhi, rlo);

    assert_approx_equal(rhi[0] + rlo[0], (ahi[0] + alo[0]) / (bhi[0] + blo[0]), "div 10/2", 1e-10);
    assert_approx_equal(rhi[1] + rlo[1], (ahi[1] + alo[1]) / (bhi[1] + blo[1]), "div 1/0", 1e-10);
    assert_approx_equal(rhi[2] + rlo[2], (ahi[2] + alo[2]) / (bhi[2] + blo[2]), "div 0/0", 1e-10);
    assert_approx_equal(rhi[3] + rlo[3], (ahi[3] + alo[3]) / (bhi[3] + blo[3]), "div Inf/Inf",
                        1e-10);
}

/*
 * [TEST CASE] simd square root
 * tests the functionality of simd double-single square root.
 */
void test_sqrt() {
    printf("Testing simd_f128x4_sqrt...\n");
    simd_f128x4 a, res;
    double ahi[4] = {4.0, -1.0, 0.0, INFINITY};
    double alo[4] = {0.0, 0.0, 0.0, 0.0};

    a.hi = _mm256_loadu_pd(ahi);
    a.lo = _mm256_loadu_pd(alo);

    res = simd_f128x4_sqrt(a);

    double rhi[4], rlo[4];
    extract_simd_f128x4(res, rhi, rlo);

    assert_approx_equal(rhi[0] + rlo[0], sqrt(ahi[0] + alo[0]), "sqrt 4", 1e-10);
    assert_approx_equal(rhi[1] + rlo[1], sqrt(ahi[1] + alo[1]), "sqrt -1", 1e-10);
    assert_approx_equal(rhi[2] + rlo[2], sqrt(ahi[2] + alo[2]), "sqrt 0", 1e-10);
    assert_approx_equal(rhi[3] + rlo[3], sqrt(ahi[3] + alo[3]), "sqrt Inf", 1e-10);
}
#endif

int main() {
#ifdef USE_SIMD_F128
    test_div();
    test_sqrt();
    printf("All SIMD math tests passed.\n");
#else
    printf("SIMD F128 not enabled. Skipping tests.\n");
#endif
    return 0;
}
