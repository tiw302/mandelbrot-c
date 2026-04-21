#include "mandelbrot.h"
#include <math.h>
#include <stdint.h>
#include <immintrin.h>

double mandelbrot_check(complex_t c, int max_iterations) {
    double cr_minus_025 = c.re - 0.25;
    double im_sq = c.im * c.im;
    double q = cr_minus_025 * cr_minus_025 + im_sq;
    if (q * (q + cr_minus_025) <= 0.25 * im_sq) {
        return (double)max_iterations;
    }

    double cr_plus_1 = c.re + 1.0;
    if (cr_plus_1 * cr_plus_1 + im_sq <= 0.0625) {
        return (double)max_iterations;
    }

    complex_t z = {0, 0};
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < max_iterations) {
        double zre2 = z.re * z.re;
        double zim2 = z.im * z.im;
        
        if (zre2 + zim2 > escape_radius_sq) {
            return (double)iterations + 2.0 - log2(log(zre2 + zim2));
        }

        z.im = 2.0 * z.re * z.im + c.im;
        z.re = zre2 - zim2 + c.re;
        iterations++;
    }

    return (double)max_iterations;
}

#ifdef __AVX2__
void mandelbrot_check_avx2(const double *re, const double *im, int max_iterations, double *results) {
    __m256d cre = _mm256_loadu_pd(re);
    __m256d cim = _mm256_loadu_pd(im);

    __m256d cre_m_025 = _mm256_sub_pd(cre, _mm256_set1_pd(0.25));
    __m256d cim2 = _mm256_mul_pd(cim, cim);
    __m256d q = _mm256_add_pd(_mm256_mul_pd(cre_m_025, cre_m_025), cim2);
    __m256d cardioid_mask = _mm256_cmp_pd(_mm256_mul_pd(q, _mm256_add_pd(q, cre_m_025)),
                                         _mm256_mul_pd(_mm256_set1_pd(0.25), cim2), _CMP_LE_OQ);
    __m256d cre_p_1 = _mm256_add_pd(cre, _mm256_set1_pd(1.0));
    __m256d bulb_mask = _mm256_cmp_pd(_mm256_add_pd(_mm256_mul_pd(cre_p_1, cre_p_1), cim2),
                                      _mm256_set1_pd(0.0625), _CMP_LE_OQ);
    __m256d in_set_mask = _mm256_or_pd(cardioid_mask, bulb_mask);

    __m256d zre = _mm256_setzero_pd();
    __m256d zim = _mm256_setzero_pd();
    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = in_set_mask;
    __m256d final_mag_sq = _mm256_setzero_pd();
    __m256d one = _mm256_set1_pd(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (_mm256_movemask_pd(escaped_mask) == 0xF) break;

        __m256d zre2 = _mm256_mul_pd(zre, zre);
        __m256d zim2 = _mm256_mul_pd(zim, zim);
        __m256d mag_sq = _mm256_add_pd(zre2, zim2);

        __m256d mask = _mm256_cmp_pd(mag_sq, esc_radius_sq, _CMP_GT_OQ);
        __m256d just_escaped = _mm256_andnot_pd(escaped_mask, mask);

        final_mag_sq = _mm256_or_pd(final_mag_sq, _mm256_and_pd(just_escaped, mag_sq));
        escaped_mask = _mm256_or_pd(escaped_mask, mask);

        iters = _mm256_add_pd(iters, _mm256_andnot_pd(escaped_mask, one));

        __m256d zre_zim = _mm256_mul_pd(zre, zim);
        zim = _mm256_add_pd(_mm256_add_pd(zre_zim, zre_zim), cim);
        zre = _mm256_add_pd(_mm256_sub_pd(zre2, zim2), cre);
    }

    double res_iters[4], res_mag_sq[4];
    uint64_t res_in_set[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);
    _mm256_storeu_pd((double*)res_in_set, in_set_mask);

    for (int i = 0; i < 4; i++) {
        if (res_in_set[i] || res_iters[i] >= max_iterations - 1) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(res_mag_sq[i]));
        }
    }
}
#endif

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
void mandelbrot_check_wasm_simd128(const double *re, const double *im, int max_iterations, double *results) {
    v128_t cre = wasm_v128_load(re);
    v128_t cim = wasm_v128_load(im);

    v128_t cre_m_025 = wasm_f64x2_sub(cre, wasm_f64x2_splat(0.25));
    v128_t cim2 = wasm_f64x2_mul(cim, cim);
    v128_t q = wasm_f64x2_add(wasm_f64x2_mul(cre_m_025, cre_m_025), cim2);
    v128_t cardioid_mask = wasm_f64x2_le(wasm_f64x2_mul(q, wasm_f64x2_add(q, cre_m_025)),
                                        wasm_f64x2_mul(wasm_f64x2_splat(0.25), cim2));

    v128_t cre_p_1 = wasm_f64x2_add(cre, wasm_f64x2_splat(1.0));
    v128_t bulb_mask = wasm_f64x2_le(wasm_f64x2_add(wasm_f64x2_mul(cre_p_1, cre_p_1), cim2),
                                     wasm_f64x2_splat(0.0625));

    v128_t in_set_mask = wasm_v128_or(cardioid_mask, bulb_mask);

    v128_t zre = wasm_f64x2_splat(0.0);
    v128_t zim = wasm_f64x2_splat(0.0);
    v128_t iters = wasm_f64x2_splat(0.0);
    v128_t esc_radius_sq = wasm_f64x2_splat(ESCAPE_RADIUS * ESCAPE_RADIUS);
    v128_t escaped_mask = in_set_mask;
    v128_t final_mag_sq = wasm_f64x2_splat(0.0);
    v128_t one = wasm_f64x2_splat(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (wasm_i64x2_all_true(escaped_mask)) break;

        v128_t zre2 = wasm_f64x2_mul(zre, zre);
        v128_t zim2 = wasm_f64x2_mul(zim, zim);
        v128_t mag_sq = wasm_f64x2_add(zre2, zim2);
        
        v128_t mask = wasm_f64x2_gt(mag_sq, esc_radius_sq);
        v128_t just_escaped = wasm_v128_andnot(mask, escaped_mask);

        final_mag_sq = wasm_v128_or(final_mag_sq, wasm_v128_and(just_escaped, mag_sq));
        escaped_mask = wasm_v128_or(escaped_mask, mask);
        iters = wasm_f64x2_add(iters, wasm_v128_andnot(one, escaped_mask));

        v128_t zre_zim = wasm_f64x2_mul(zre, zim);
        zim = wasm_f64x2_add(wasm_f64x2_add(zre_zim, zre_zim), cim);
        zre = wasm_f64x2_add(wasm_f64x2_sub(zre2, zim2), cre);
    }

    double res_iters[2], res_mag_sq[2];
    uint64_t res_in_set[2];
    wasm_v128_store(res_iters, iters);
    wasm_v128_store(res_mag_sq, final_mag_sq);
    wasm_v128_store(res_in_set, in_set_mask);

    for (int i = 0; i < 2; i++) {
        if (res_in_set[i] || res_iters[i] >= max_iterations - 1) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(res_mag_sq[i]));
        }
    }
}
#endif