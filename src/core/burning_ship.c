#include "core_math.h"
#include "burning_ship.h"

#ifdef __AVX2__
#include <immintrin.h>
#endif
#include <math.h>
#include <stdint.h>

/* scalar burning ship path:
 * identical to mandelbrot except z.re and z.im are replaced with
 * their absolute values before the squaring step. this single change
 * produces a dramatically different fractal shape. */
double burning_ship_check(complex_t c, int max_iterations) {
    complex_t z = {0, 0};
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < max_iterations) {
        double zre2 = z.re * z.re;
        double zim2 = z.im * z.im;

        if (zre2 + zim2 > escape_radius_sq) {
            /* smooth coloring — same formula as mandelbrot */
            return (double)iterations + 2.0 - log2(log(fmax(1.0, zre2 + zim2)));
        }

        /* the key difference: take absolute values before squaring */
        double abs_re = fabs(z.re);
        double abs_im = fabs(z.im);
        z.im = 2.0 * abs_re * abs_im + c.im;
        z.re = zre2 - zim2 + c.re;
        iterations++;
    }

    return (double)max_iterations;
}

#ifdef __AVX2__
/* avx2 vectorized burning ship path:
 * processes 4 pixels simultaneously using _mm256_andnot_pd to
 * clear the sign bit for the absolute value operation. */
void burning_ship_check_avx2(const double* re, const double* im, int max_iterations,
                              double* results) {
    __m256d cre = _mm256_loadu_pd(re);
    __m256d cim = _mm256_loadu_pd(im);

    __m256d zre = _mm256_setzero_pd();
    __m256d zim = _mm256_setzero_pd();
    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = _mm256_setzero_pd();
    __m256d final_mag_sq = _mm256_setzero_pd();
    __m256d one = _mm256_set1_pd(1.0);

    /* sign mask: all bits set except the sign bit — used for fabs */
    __m256d sign_mask = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFLL));

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

        /* absolute values via bitwise AND with sign mask */
        __m256d abs_re = _mm256_and_pd(zre, sign_mask);
        __m256d abs_im = _mm256_and_pd(zim, sign_mask);

        __m256d zre_zim = _mm256_mul_pd(abs_re, abs_im);
        zim = _mm256_add_pd(_mm256_add_pd(zre_zim, zre_zim), cim);
        zre = _mm256_add_pd(_mm256_sub_pd(zre2, zim2), cre);
    }

    double res_iters[4], res_mag_sq[4];
    uint64_t res_escaped[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);
    _mm256_storeu_pd((double*)res_escaped, escaped_mask);

    for (int i = 0; i < 4; i++) {
        if (!res_escaped[i] || res_iters[i] >= max_iterations - 1) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(1.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
/* wasm simd128 vectorized burning ship path:
 * processes 2 pixels simultaneously for browser execution. */
void burning_ship_check_wasm_simd128(const double* re, const double* im, int max_iterations,
                                      double* results) {
    v128_t cre = wasm_v128_load(re);
    v128_t cim = wasm_v128_load(im);

    v128_t zre = wasm_f64x2_splat(0.0);
    v128_t zim = wasm_f64x2_splat(0.0);
    v128_t iters = wasm_f64x2_splat(0.0);
    v128_t esc_radius_sq = wasm_f64x2_splat(ESCAPE_RADIUS * ESCAPE_RADIUS);
    v128_t escaped_mask = wasm_i64x2_make(0, 0);
    v128_t final_mag_sq = wasm_f64x2_splat(0.0);
    v128_t one = wasm_f64x2_splat(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (wasm_i64x2_all_true(escaped_mask)) break;

        v128_t zre2 = wasm_f64x2_mul(zre, zre);
        v128_t zim2 = wasm_f64x2_mul(zim, zim);
        v128_t mag_sq = wasm_f64x2_add(zre2, zim2);

        v128_t mask = wasm_f64x2_gt(mag_sq, esc_radius_sq);

        /* wasm_v128_andnot(a,b) = a & ~b — opposite of intel _mm256_andnot_pd(a,b) = ~a & b.
         * arguments are intentionally swapped vs the avx2 path. */
        v128_t just_escaped = wasm_v128_andnot(mask, escaped_mask);

        final_mag_sq = wasm_v128_or(final_mag_sq, wasm_v128_and(just_escaped, mag_sq));
        escaped_mask = wasm_v128_or(escaped_mask, mask);
        iters = wasm_f64x2_add(iters, wasm_v128_andnot(one, escaped_mask));

        /* absolute values */
        v128_t abs_re = wasm_f64x2_abs(zre);
        v128_t abs_im = wasm_f64x2_abs(zim);

        v128_t zre_zim = wasm_f64x2_mul(abs_re, abs_im);
        zim = wasm_f64x2_add(wasm_f64x2_add(zre_zim, zre_zim), cim);
        zre = wasm_f64x2_add(wasm_f64x2_sub(zre2, zim2), cre);
    }

    double res_iters[2], res_mag_sq[2];
    uint64_t res_escaped[2];
    wasm_v128_store(res_iters, iters);
    wasm_v128_store(res_mag_sq, final_mag_sq);
    wasm_v128_store(res_escaped, escaped_mask);

    for (int i = 0; i < 2; i++) {
        if (!res_escaped[i] || res_iters[i] >= max_iterations - 1) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(1.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef USE_SIMD_F128
static inline simd_f128 f128_abs(simd_f128 x) {
    double hi, lo;
    simd_f128_extract(x, &hi, &lo);
    if (hi < 0.0) {
#if defined(SIMD_F128_USE_AVX2) || defined(SIMD_F128_USE_SSE2)
        return _mm_xor_pd(x, _mm_set1_pd(-0.0));
#elif defined(SIMD_F128_USE_WASM)
        v128_t neg_mask = wasm_i64x2_const(0x8000000000000000ULL, 0x8000000000000000ULL);
        return wasm_v128_xor(x, neg_mask);
#elif defined(SIMD_F128_USE_NEON)
        return vnegq_f64(x);
#else
        simd_f128 res = {-hi, -lo};
        return res;
#endif
    }
    return x;
}

double burning_ship_check_f128(simd_f128 cre, simd_f128 cim, int max_iterations) {
    simd_f128 zre = simd_f128_from_double(0.0);
    simd_f128 zim = simd_f128_from_double(0.0);
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < max_iterations) {
        simd_f128 zre2 = simd_f128_sqr(zre);
        simd_f128 zim2 = simd_f128_sqr(zim);
        
        double mag_hi = simd_f128_get_hi(zre2) + simd_f128_get_hi(zim2);
        
        if (mag_hi > escape_radius_sq) {
            return (double)iterations + 2.0 - log2(log(fmax(1.0, mag_hi)));
        }
        
        simd_f128 abs_re = f128_abs(zre);
        simd_f128 abs_im = f128_abs(zim);
        
        simd_f128 zre_zim = simd_f128_mul(abs_re, abs_im);
        zim = simd_f128_add(simd_f128_mul2(zre_zim), cim);
        zre = simd_f128_add(simd_f128_sub(zre2, zim2), cre);
        
        iterations++;
    }
    return (double)max_iterations;
}
#endif
