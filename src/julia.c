#include "julia.h"
#include <math.h>

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
void julia_check_wasm_simd128(const double *re, const double *im, complex_t c, int max_iterations, double *results) {
    v128_t cre = wasm_f64x2_splat(c.re);
    v128_t cim = wasm_f64x2_splat(c.im);
    v128_t zre = wasm_v128_load(re);
    v128_t zim = wasm_v128_load(im);

    v128_t iters = wasm_f64x2_splat(0.0);
    v128_t esc_radius_sq = wasm_f64x2_splat(ESCAPE_RADIUS * ESCAPE_RADIUS);
    v128_t escaped_mask = wasm_i64x2_make(0, 0);
    v128_t final_mag_sq = wasm_f64x2_splat(0.0);

    for (int i = 0; i < max_iterations; i++) {
        if (wasm_i64x2_all_true(wasm_v128_and(escaped_mask, wasm_i64x2_make(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF)))) break;

        v128_t zre2 = wasm_f64x2_mul(zre, zre);
        v128_t zim2 = wasm_f64x2_mul(zim, zim);
        v128_t next_re = wasm_f64x2_add(wasm_f64x2_sub(zre2, zim2), cre);
        v128_t next_im = wasm_f64x2_add(wasm_f64x2_mul(wasm_f64x2_splat(2.0), wasm_f64x2_mul(zre, zim)), cim);
        zre = next_re;
        zim = next_im;

        v128_t mag_sq = wasm_f64x2_add(wasm_f64x2_mul(zre, zre), wasm_f64x2_mul(zim, zim));
        v128_t mask = wasm_f64x2_gt(mag_sq, esc_radius_sq);
        v128_t just_escaped = wasm_v128_andnot(mask, escaped_mask);
        
        final_mag_sq = wasm_v128_or(final_mag_sq, wasm_v128_and(just_escaped, mag_sq));
        escaped_mask = wasm_v128_or(escaped_mask, mask);
        iters = wasm_f64x2_add(iters, wasm_v128_andnot(wasm_f64x2_splat(1.0), escaped_mask));
    }

    double res_iters[2], res_mag_sq[2];
    wasm_v128_store(res_iters, iters);
    wasm_v128_store(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 2; i++) {
        if (res_iters[i] >= max_iterations - 1) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(res_mag_sq[i]));
        }
    }
}
#endif

double julia_check(complex_t z, complex_t c, int max_iterations) {
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < max_iterations) {
        /* next z */
        double next_re = z.re * z.re - z.im * z.im + c.re;
        double next_im = 2 * z.re * z.im + c.im;
        z.re = next_re;
        z.im = next_im;

        double mag_sq = z.re * z.re + z.im * z.im;
        if (mag_sq > escape_radius_sq) {
            /* smooth color */
            return (double)iterations + 2.0 - log2(log(mag_sq));
        }

        iterations++;
    }

    return (double)max_iterations;
}

#ifdef __AVX2__
void julia_check_avx2(const double *re, const double *im, complex_t c, int max_iterations, double *results) {
    __m256d cre = _mm256_set1_pd(c.re);
    __m256d cim = _mm256_set1_pd(c.im);
    __m256d zre = _mm256_loadu_pd(re);
    __m256d zim = _mm256_loadu_pd(im);

    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = _mm256_setzero_pd();
    __m256d final_mag_sq = _mm256_setzero_pd();

    for (int i = 0; i < max_iterations; i++) {
        if (_mm256_movemask_pd(escaped_mask) == 0xF) break;

        __m256d zre2 = _mm256_mul_pd(zre, zre);
        __m256d zim2 = _mm256_mul_pd(zim, zim);
        
        __m256d next_re = _mm256_add_pd(_mm256_sub_pd(zre2, zim2), cre);
        __m256d next_im = _mm256_add_pd(_mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_mul_pd(zre, zim)), cim);
        zre = next_re;
        zim = next_im;

        __m256d mag_sq = _mm256_add_pd(_mm256_mul_pd(zre, zre), _mm256_mul_pd(zim, zim));
        __m256d mask = _mm256_cmp_pd(mag_sq, esc_radius_sq, _CMP_GT_OQ);
        __m256d just_escaped = _mm256_andnot_pd(escaped_mask, mask);

        final_mag_sq = _mm256_or_pd(final_mag_sq, _mm256_and_pd(just_escaped, mag_sq));
        escaped_mask = _mm256_or_pd(escaped_mask, mask);

        iters = _mm256_add_pd(iters, _mm256_andnot_pd(escaped_mask, _mm256_set1_pd(1.0)));
    }

    double res_iters[4], res_mag_sq[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 4; i++) {
        if (res_iters[i] >= max_iterations - 1) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(res_mag_sq[i]));
        }
    }
}
#endif
