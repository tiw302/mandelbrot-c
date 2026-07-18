/* julia.c
 *
 * mathematical kernels for computing the julia set of mandelbrot.
 * supports scalar, avx2, avx512, neon, wasm, and 128-bit precision.
 *
 * formula: z_{n+1} = z_{n}^2 + c (where c is a constant parameter tracked by the mouse cursor)
 *
 * paths provided:
 *   - julia_double     — standard scalar double-precision loop
 *   - julia_simd_f64   — vectorized double-precision using avx2/avx512/neon/wasm
 *   - julia_f128       — high-precision 128-bit float using __float128
 *   - julia_simd_f128  — vectorized 128-bit using custom SIMD math wrappers
 */

#include "core_math.h"

#if defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h>
#endif
#include <math.h>

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
/* wasm simd128 vectorized julia path:
 * calculates julia set iterations for 2 pixels concurrently.
 * the c parameter is constant across all pixels for julia sets. */
void julia_check_wasm_simd128(v128_t zre, v128_t zim, complex_t c, int max_iterations,
                              double* results) {
    v128_t cre = wasm_f64x2_splat(c.re);
    v128_t cim = wasm_f64x2_splat(c.im);

    v128_t iters = wasm_f64x2_splat(0.0);
    v128_t esc_radius_sq = wasm_f64x2_splat(ESCAPE_RADIUS * ESCAPE_RADIUS);
    v128_t escaped_mask = wasm_i64x2_make(0, 0);
    v128_t final_mag_sq = wasm_f64x2_splat(0.0);

    for (int i = 0; i < max_iterations; i++) {
        if (wasm_i64x2_all_true(escaped_mask)) break;

        v128_t zre2 = wasm_f64x2_mul(zre, zre);
        v128_t zim2 = wasm_f64x2_mul(zim, zim);
        v128_t mag_sq = wasm_f64x2_add(zre2, zim2);

        v128_t mask = wasm_f64x2_gt(mag_sq, esc_radius_sq);

        /* wasm_v128_andnot(a,b) = a & ~b — opposite of intel _mm256_andnot_pd(a,b) = ~a & b.
         * * arguments are intentionally swapped vs the avx2 path. */
        v128_t just_escaped = wasm_v128_andnot(mask, escaped_mask);

        final_mag_sq = wasm_v128_or(final_mag_sq, wasm_v128_and(just_escaped, mag_sq));
        escaped_mask = wasm_v128_or(escaped_mask, mask);
        iters = wasm_f64x2_add(iters, wasm_v128_andnot(wasm_f64x2_splat(1.0), escaped_mask));

        v128_t next_im =
            wasm_f64x2_add(wasm_f64x2_mul(wasm_f64x2_splat(2.0), wasm_f64x2_mul(zre, zim)), cim);
        zre = wasm_f64x2_add(wasm_f64x2_sub(zre2, zim2), cre);
        zim = next_im;
    }

    double res_iters[2], res_mag_sq[2];
    wasm_v128_store(res_iters, iters);
    wasm_v128_store(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 2; i++) {
        if (res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef __AVX512F__
/* avx-512 vectorized julia path: * processes 8 pixels simultaneously for maximum desktop cpu
 * throughput. */
void julia_check_avx512(__m512d zre, __m512d zim, complex_t c, int max_iterations,
                        double* results) {
    __m512d cre = _mm512_set1_pd(c.re);
    __m512d cim = _mm512_set1_pd(c.im);

    __m512d iters = _mm512_setzero_pd();
    __m512d esc_radius_sq = _mm512_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __mmask8 escaped_mask = 0;
    __m512d final_mag_sq = _mm512_setzero_pd();

    for (int i = 0; i < max_iterations; i++) {
        if (escaped_mask == 0xFF) break;

        __m512d zre2 = _mm512_mul_pd(zre, zre);
        __m512d zim2 = _mm512_mul_pd(zim, zim);
        __m512d mag_sq = _mm512_add_pd(zre2, zim2);

        __mmask8 mask = _mm512_cmp_pd_mask(mag_sq, esc_radius_sq, _CMP_GT_OQ);
        __mmask8 just_escaped = mask & ~escaped_mask;

        final_mag_sq = _mm512_mask_blend_pd(just_escaped, final_mag_sq, mag_sq);
        escaped_mask |= mask;

        iters = _mm512_mask_add_pd(iters, ~escaped_mask, iters, _mm512_set1_pd(1.0));

        __m512d next_im =
            _mm512_add_pd(_mm512_mul_pd(_mm512_set1_pd(2.0), _mm512_mul_pd(zre, zim)), cim);
        zre = _mm512_add_pd(_mm512_sub_pd(zre2, zim2), cre);
        zim = next_im;
    }

    double res_iters[8], res_mag_sq[8];
    _mm512_storeu_pd(res_iters, iters);
    _mm512_storeu_pd(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 8; i++) {
        if (res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif

/* scalar julia path:
 * unlike mandelbrot, julia sets use the pixel coordinate as the initial z,
 * and keep the complex parameter c completely constant for the whole fractal. */
double julia_check(complex_t z, complex_t c, int max_iterations) {
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < max_iterations) {
        double zre2 = z.re * z.re;
        double zim2 = z.im * z.im;
        double mag_sq = zre2 + zim2;
        if (mag_sq > escape_radius_sq) {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            return (double)iterations + 2.0 - log2(log(fmax(2.0, mag_sq)));
        }

        double next_im = 2.0 * z.re * z.im + c.im;
        z.re = zre2 - zim2 + c.re;
        z.im = next_im;

        iterations++;
    }

    return (double)max_iterations;
}

#ifdef __AVX2__
/* avx2 vectorized julia path: * processes 4 pixels simultaneously for maximum desktop cpu
 * throughput. */
void julia_check_avx2(__m256d zre, __m256d zim, complex_t c, int max_iterations, double* results) {
    __m256d cre = _mm256_set1_pd(c.re);
    __m256d cim = _mm256_set1_pd(c.im);

    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = _mm256_setzero_pd();
    __m256d final_mag_sq = _mm256_setzero_pd();

    for (int i = 0; i < max_iterations; i++) {
        if (_mm256_movemask_pd(escaped_mask) == 0xF) break;

        __m256d zre2 = _mm256_mul_pd(zre, zre);
        __m256d zim2 = _mm256_mul_pd(zim, zim);
        __m256d mag_sq = _mm256_add_pd(zre2, zim2);

        __m256d mask = _mm256_cmp_pd(mag_sq, esc_radius_sq, _CMP_GT_OQ);
        __m256d just_escaped = _mm256_andnot_pd(escaped_mask, mask);

        final_mag_sq = _mm256_or_pd(final_mag_sq, _mm256_and_pd(just_escaped, mag_sq));
        escaped_mask = _mm256_or_pd(escaped_mask, mask);

        iters = _mm256_add_pd(iters, _mm256_andnot_pd(escaped_mask, _mm256_set1_pd(1.0)));

        __m256d next_im =
            _mm256_add_pd(_mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_mul_pd(zre, zim)), cim);
        zre = _mm256_add_pd(_mm256_sub_pd(zre2, zim2), cre);
        zim = next_im;
    }

    double res_iters[4], res_mag_sq[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 4; i++) {
        if (res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
/* arm neon vectorized path: * processes 2 pixels simultaneously using 64-bit float vectors. */
void julia_check_neon(float64x2_t zre, float64x2_t zim, float64x2_t cre, float64x2_t cim,
                      int max_iterations, double* results) {
    float64x2_t iters = vdupq_n_f64(0.0);
    float64x2_t esc_radius_sq = vdupq_n_f64(ESCAPE_RADIUS * ESCAPE_RADIUS);
    uint64x2_t escaped_mask = vdupq_n_u64(0);
    float64x2_t final_mag_sq = vdupq_n_f64(0.0);
    float64x2_t one = vdupq_n_f64(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (vgetq_lane_u64(escaped_mask, 0) == ~0ULL && vgetq_lane_u64(escaped_mask, 1) == ~0ULL)
            break;

        float64x2_t zre2 = vmulq_f64(zre, zre);
        float64x2_t zim2 = vmulq_f64(zim, zim);
        float64x2_t mag_sq = vaddq_f64(zre2, zim2);

        uint64x2_t mask = vcgtq_f64(mag_sq, esc_radius_sq);
        uint64x2_t just_escaped = vbicq_u64(mask, escaped_mask);

        final_mag_sq = vbslq_f64(just_escaped, mag_sq, final_mag_sq);
        escaped_mask = vorrq_u64(escaped_mask, mask);

        float64x2_t iters_inc = vbslq_f64(escaped_mask, vdupq_n_f64(0.0), one);
        iters = vaddq_f64(iters, iters_inc);

        float64x2_t zre_zim = vmulq_f64(zre, zim);
        float64x2_t next_im = vaddq_f64(vaddq_f64(zre_zim, zre_zim), cim);
        zre = vaddq_f64(vsubq_f64(zre2, zim2), cre);
        zim = next_im;
    }

    double res_iters[2], res_mag_sq[2];
    vst1q_f64(res_iters, iters);
    vst1q_f64(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 2; i++) {
        if (res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef USE_SIMD_F128
/* high-precision 128-bit julia path:
 *
 * prevents pixelation for extreme deep zooms in julia mode.
 * emulates 128-bit floats by splitting numbers into a high part (most significant)
 * and a low part (residual error). this allows zooming beyond the 10^14 limit.
 */
double julia_check_f128(simd_f128 zre, simd_f128 zim, simd_f128 cre, simd_f128 cim,
                        int max_iterations) {
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    /* core 128-bit double-double iteration loop.
     * arithmetic functions (add, sub, mul, sqr) handle error propagation
     * using dekker/knuth split techniques on double-precision pairs. */
    while (iterations < max_iterations) {
        simd_f128 zre2 = simd_f128_mul(zre, zre);
        simd_f128 zim2 = simd_f128_mul(zim, zim);
        simd_f128 mag_sq = simd_f128_add(zre2, zim2);

        double mag_hi, mag_lo;
        simd_f128_extract(mag_sq, &mag_hi, &mag_lo);

        if (mag_hi > escape_radius_sq) {
            /* smooth coloring algorithm.
             * guards against log2(0) by using fmax(2.0, mag_hi). */
            return (double)iterations + 2.0 - log2(log(fmax(2.0, mag_hi)));
        }

        // z = z^2 + c -> z_re = zre^2 - zim^2 + cre, z_im = 2*zre*zim + cim
        simd_f128 zre_zim = simd_f128_mul(zre, zim);
        simd_f128 next_im = simd_f128_add(simd_f128_add(zre_zim, zre_zim), cim);
        zre = simd_f128_add(simd_f128_sub(zre2, zim2), cre);
        zim = next_im;

        iterations++;
    }

    return (double)max_iterations;
}

#ifdef __AVX2__
/* avx2 high-precision 128-bit julia path:
 *
 * processes 4 pixels simultaneously with double-double precision.
 * prevents pixelation for extremely deep zooms with high throughput.
 * uses 4-way registers to run double-double arithmetic in parallel.
 */
void julia_check_f128x4(simd_f128x4 zre, simd_f128x4 zim, simd_f128x4 cre, simd_f128x4 cim,
                        int max_iterations, double* results) {
    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = _mm256_setzero_pd();
    __m256d final_mag_sq = _mm256_setzero_pd();
    __m256d one = _mm256_set1_pd(1.0);

    for (int i = 0; i < max_iterations; i++) {
        // movemask checks if all 4 pixels have escaped; if so, aborts early
        if (_mm256_movemask_pd(escaped_mask) == 0xF) break;

        simd_f128x4 zre2 = simd_f128x4_sqr(zre);
        simd_f128x4 zim2 = simd_f128x4_sqr(zim);
        simd_f128x4 mag_sq = simd_f128x4_add(zre2, zim2);
        __m256d mag_hi = mag_sq.hi;

        __m256d mask = _mm256_cmp_pd(mag_hi, esc_radius_sq, _CMP_GT_OQ);
        __m256d just_escaped = _mm256_andnot_pd(escaped_mask, mask);

        // capture magnitude at escape point for smooth coloring calculations
        final_mag_sq = _mm256_or_pd(final_mag_sq, _mm256_and_pd(just_escaped, mag_hi));
        escaped_mask = _mm256_or_pd(escaped_mask, mask);

        // increment iteration counters only for active lanes
        iters = _mm256_add_pd(iters, _mm256_andnot_pd(escaped_mask, one));

        simd_f128x4 next_im = simd_f128x4_add(simd_f128x4_mul2(simd_f128x4_mul(zre, zim)), cim);
        zre = simd_f128x4_add(simd_f128x4_sub(zre2, zim2), cre);
        zim = next_im;
    }

    double res_iters[4], res_mag_sq[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 4; i++) {
        if (res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif
#endif
