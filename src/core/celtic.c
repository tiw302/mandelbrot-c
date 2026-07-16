/* celtic.c
 *
 * mathematical kernel for computing the celtic fractal.
 * implements abs-based real parts in scalar, simd, and 128-bit paths.
 */

#include "core_math.h"

#if defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h>
#endif
#include <math.h>
#include <stdint.h>

/* scalar celtic path:
 * calculates iterations for a single pixel.
 * uses early rejection to skip points inside the main cardioid and period-2 bulb. */
double celtic_check(complex_t c, int max_iterations) {
    /* early rejection for the main cardioid and period-2 bulb.
     * points in these regions are guaranteed to be in the set (infinite iterations).
     * skipping them saves massive amounts of cpu cycles in the dark center. */

    complex_t z = {0, 0};
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    // core iterative loop: z = z^2 + c
    // using x^2 and y^2 saves one multiplication per iteration.
    while (iterations < max_iterations) {
        double zre2 = z.re * z.re;
        double zim2 = z.im * z.im;

        double mag_sq = zre2 + zim2;
        if (mag_sq > escape_radius_sq) {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            return (double)iterations + 2.0 - log2(log(fmax(2.0, mag_sq)));
        }

        z.im = 2.0 * z.re * z.im + c.im;
        z.re = fabs(zre2 - zim2) + c.re;
        iterations++;
    }

    return (double)max_iterations;
}

#ifdef __AVX2__
/* avx2 vectorized path:
 * processes 4 pixels simultaneously. employs a parallel bitmask
 * to track which pixels have escaped, stopping only when all 4 are done. */
void celtic_check_avx2(__m256d cre, __m256d cim, int max_iterations, double* results) {
    __m256d cre_m_025 = _mm256_sub_pd(cre, _mm256_set1_pd(0.25));
    __m256d cim2 = _mm256_mul_pd(cim, cim);
    __m256d q = _mm256_add_pd(_mm256_mul_pd(cre_m_025, cre_m_025), cim2);
    __m256d cardioid_mask = _mm256_setzero_pd();
    __m256d cre_p_1 = _mm256_add_pd(cre, _mm256_set1_pd(1.0));
    __m256d bulb_mask = _mm256_setzero_pd();
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
        __m256d zre2_zim2 = _mm256_sub_pd(zre2, zim2);
        __m256d abs_mask = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFULL));
        zre = _mm256_add_pd(_mm256_and_pd(zre2_zim2, abs_mask), cre);
    }

    double res_iters[4], res_mag_sq[4];
    _mm256_storeu_pd(res_iters, iters);
    _mm256_storeu_pd(res_mag_sq, final_mag_sq);

    // use movemask to avoid strict aliasing ub from double* punning
    int in_set_bits = _mm256_movemask_pd(in_set_mask);

    for (int i = 0; i < 4; i++) {
        if ((in_set_bits & (1 << i)) || res_iters[i] >= max_iterations) {
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
/* avx-512 vectorized path:
 * processes 8 pixels simultaneously. employs a parallel bitmask
 * to track which pixels have escaped, stopping only when all 8 are done. */
void celtic_check_avx512(__m512d cre, __m512d cim, int max_iterations, double* results) {
    __m512d cre_m_025 = _mm512_sub_pd(cre, _mm512_set1_pd(0.25));
    __m512d cim2 = _mm512_mul_pd(cim, cim);
    __m512d q = _mm512_add_pd(_mm512_mul_pd(cre_m_025, cre_m_025), cim2);
    __mmask8 cardioid_mask = 0;
    __m512d cre_p_1 = _mm512_add_pd(cre, _mm512_set1_pd(1.0));
    __mmask8 bulb_mask = 0;
    __mmask8 in_set_mask = cardioid_mask | bulb_mask;

    __m512d zre = _mm512_setzero_pd();
    __m512d zim = _mm512_setzero_pd();
    __m512d iters = _mm512_setzero_pd();
    __m512d esc_radius_sq = _mm512_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __mmask8 escaped_mask = in_set_mask;
    __m512d final_mag_sq = _mm512_setzero_pd();
    __m512d one = _mm512_set1_pd(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (escaped_mask == 0xFF) break;

        __m512d zre2 = _mm512_mul_pd(zre, zre);
        __m512d zim2 = _mm512_mul_pd(zim, zim);
        __m512d mag_sq = _mm512_add_pd(zre2, zim2);

        __mmask8 mask = _mm512_cmp_pd_mask(mag_sq, esc_radius_sq, _CMP_GT_OQ);
        __mmask8 just_escaped = mask & ~escaped_mask;

        final_mag_sq = _mm512_mask_blend_pd(just_escaped, final_mag_sq, mag_sq);
        escaped_mask |= mask;

        iters = _mm512_mask_add_pd(iters, ~escaped_mask, iters, one);

        __m512d zre_zim = _mm512_mul_pd(zre, zim);
        zim = _mm512_add_pd(_mm512_add_pd(zre_zim, zre_zim), cim);
        __m512d zre2_zim2 = _mm512_sub_pd(zre2, zim2);
        __m512d abs_mask = _mm512_castsi512_pd(_mm512_set1_epi64(0x7FFFFFFFFFFFFFFFULL));
        zre = _mm512_add_pd(_mm512_and_pd(zre2_zim2, abs_mask), cre);
    }

    double res_iters[8], res_mag_sq[8];
    _mm512_storeu_pd(res_iters, iters);
    _mm512_storeu_pd(res_mag_sq, final_mag_sq);

    for (int i = 0; i < 8; i++) {
        if ((in_set_mask & (1 << i)) || res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
// wasm simd128 vectorized path:
// processes 2 pixels simultaneously for high performance in modern browsers.
void celtic_check_wasm_simd128(v128_t cre, v128_t cim, int max_iterations, double* results) {
    v128_t cre_m_025 = wasm_f64x2_sub(cre, wasm_f64x2_splat(0.25));
    v128_t cim2 = wasm_f64x2_mul(cim, cim);
    v128_t q = wasm_f64x2_add(wasm_f64x2_mul(cre_m_025, cre_m_025), cim2);
    v128_t cardioid_mask = wasm_i64x2_const(0, 0);

    v128_t cre_p_1 = wasm_f64x2_add(cre, wasm_f64x2_splat(1.0));
    v128_t bulb_mask = wasm_i64x2_const(0, 0);

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

        // wasm_v128_andnot(a,b) = a & ~b — opposite of intel _mm256_andnot_pd(a,b) = ~a & b.
        // arguments are intentionally swapped vs the avx2 path above.
        v128_t just_escaped = wasm_v128_andnot(mask, escaped_mask);

        final_mag_sq = wasm_v128_or(final_mag_sq, wasm_v128_and(just_escaped, mag_sq));
        escaped_mask = wasm_v128_or(escaped_mask, mask);
        iters = wasm_f64x2_add(iters, wasm_v128_andnot(one, escaped_mask));

        v128_t zre_zim = wasm_f64x2_mul(zre, zim);
        zim = wasm_f64x2_add(wasm_f64x2_add(zre_zim, zre_zim), cim);
        v128_t zre2_zim2 = wasm_f64x2_sub(zre2, zim2);
        zre = wasm_f64x2_add(wasm_f64x2_abs(zre2_zim2), cre);
    }

    double res_iters[2], res_mag_sq[2];
    uint64_t res_in_set[2];
    wasm_v128_store(res_iters, iters);
    wasm_v128_store(res_mag_sq, final_mag_sq);
    wasm_v128_store(res_in_set, in_set_mask);

    for (int i = 0; i < 2; i++) {
        if (res_in_set[i] || res_iters[i] >= max_iterations) {
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
// arm neon vectorized path:
// processes 2 pixels simultaneously using 64-bit float vectors.
void celtic_check_neon(float64x2_t cre, float64x2_t cim, int max_iterations, double* results) {
    float64x2_t cre_m_025 = vsubq_f64(cre, vdupq_n_f64(0.25));
    float64x2_t cim2 = vmulq_f64(cim, cim);
    float64x2_t q = vaddq_f64(vmulq_f64(cre_m_025, cre_m_025), cim2);
    uint64x2_t cardioid_mask = vdupq_n_u64(0);

    float64x2_t cre_p_1 = vaddq_f64(cre, vdupq_n_f64(1.0));
    uint64x2_t bulb_mask = vdupq_n_u64(0);

    uint64x2_t in_set_mask = vorrq_u64(cardioid_mask, bulb_mask);

    float64x2_t zre = vdupq_n_f64(0.0);
    float64x2_t zim = vdupq_n_f64(0.0);
    float64x2_t iters = vdupq_n_f64(0.0);
    float64x2_t esc_radius_sq = vdupq_n_f64(ESCAPE_RADIUS * ESCAPE_RADIUS);
    uint64x2_t escaped_mask = in_set_mask;
    float64x2_t final_mag_sq = vdupq_n_f64(0.0);
    float64x2_t one = vdupq_n_f64(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (vgetq_lane_u64(escaped_mask, 0) == ~0ULL && vgetq_lane_u64(escaped_mask, 1) == ~0ULL) break;

        float64x2_t zre2 = vmulq_f64(zre, zre);
        float64x2_t zim2 = vmulq_f64(zim, zim);
        float64x2_t mag_sq = vaddq_f64(zre2, zim2);

        uint64x2_t mask = vcgtq_f64(mag_sq, esc_radius_sq);
        uint64x2_t just_escaped = vbicq_u64(mask, escaped_mask);

        // vbslq_f64(mask, true_val, false_val) -> bitwise select
        final_mag_sq = vbslq_f64(just_escaped, mag_sq, final_mag_sq);
        escaped_mask = vorrq_u64(escaped_mask, mask);

        // iters += (1.0 where not escaped)
        float64x2_t iters_inc = vbslq_f64(escaped_mask, vdupq_n_f64(0.0), one);
        iters = vaddq_f64(iters, iters_inc);

        float64x2_t zre_zim = vmulq_f64(zre, zim);
        zim = vaddq_f64(vaddq_f64(zre_zim, zre_zim), cim);
        float64x2_t zre2_zim2 = vsubq_f64(zre2, zim2);
        zre = vaddq_f64(vabsq_f64(zre2_zim2), cre);
    }

    double res_iters[2], res_mag_sq[2];
    uint64_t res_in_set[2];
    vst1q_f64(res_iters, iters);
    vst1q_f64(res_mag_sq, final_mag_sq);
    vst1q_u64(res_in_set, in_set_mask);

    for (int i = 0; i < 2; i++) {
        if (res_in_set[i] || res_iters[i] >= max_iterations) {
            results[i] = (double)max_iterations;
        } else {
            results[i] = res_iters[i] + 2.0 - log2(log(fmax(2.0, res_mag_sq[i])));
        }
    }
}
#endif

#ifdef USE_SIMD_F128
/* high-precision 128-bit path (double-double):
 * utilizes simd intrinsics for single-pixel precision enhancement.
 * used for deep zooming (beyond 10^14) where standard 64-bit float degrades. */
double celtic_check_f128(simd_f128 cre, simd_f128 cim, int max_iterations) {
    double cre_hi, cre_lo, cim_hi, cim_lo;
    simd_f128_extract(cre, &cre_hi, &cre_lo);
    simd_f128_extract(cim, &cim_hi, &cim_lo);



    simd_f128 zre = simd_f128_from_double(0.0);
    simd_f128 zim = simd_f128_from_double(0.0);
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < max_iterations) {
        simd_f128 zre2 = simd_f128_sqr(zre);
        simd_f128 zim2 = simd_f128_sqr(zim);
        simd_f128 mag_sq = simd_f128_add(zre2, zim2);

        double mag_hi, mag_lo;
        simd_f128_extract(mag_sq, &mag_hi, &mag_lo);

        if (mag_hi > escape_radius_sq) {
            /* smooth coloring guards against log2(0) by ensuring log input is strictly > 1.
             * mag_sq is already > 100 due to the escape radius, so this is purely defensive. */
            return (double)iterations + 2.0 - log2(log(fmax(2.0, mag_hi)));
        }

        simd_f128 zre_zim = simd_f128_mul(zre, zim);
        zim = simd_f128_add(simd_f128_add(zre_zim, zre_zim), cim);
        zre = simd_f128_add(simd_f128_abs(simd_f128_sub(zre2, zim2)), cre);

        iterations++;
    }
    return (double)max_iterations;
}

#ifdef __AVX2__
/* avx2 high-precision 128-bit celtic path:
 * processes 4 pixels simultaneously with double-double precision.
 * prevents pixelation for extremely deep zooms with high throughput. */
void celtic_check_f128x4(simd_f128x4 cre, simd_f128x4 cim, int max_iterations,
                             double* results) {
    simd_f128x4 zre = simd_f128x4_from_doubles(0.0, 0.0, 0.0, 0.0);
    simd_f128x4 zim = simd_f128x4_from_doubles(0.0, 0.0, 0.0, 0.0);
    __m256d iters = _mm256_setzero_pd();
    __m256d esc_radius_sq = _mm256_set1_pd(ESCAPE_RADIUS * ESCAPE_RADIUS);
    __m256d escaped_mask = _mm256_setzero_pd();
    __m256d final_mag_sq = _mm256_setzero_pd();
    __m256d one = _mm256_set1_pd(1.0);

    for (int i = 0; i < max_iterations; i++) {
        if (_mm256_movemask_pd(escaped_mask) == 0xF) break;

        simd_f128x4 zre2 = simd_f128x4_sqr(zre);
        simd_f128x4 zim2 = simd_f128x4_sqr(zim);

        simd_f128x4 mag_sq = simd_f128x4_add(zre2, zim2);
        __m256d mag_hi = mag_sq.hi;

        __m256d mask = _mm256_cmp_pd(mag_hi, esc_radius_sq, _CMP_GT_OQ);
        __m256d just_escaped = _mm256_andnot_pd(escaped_mask, mask);

        final_mag_sq = _mm256_or_pd(final_mag_sq, _mm256_and_pd(just_escaped, mag_hi));
        escaped_mask = _mm256_or_pd(escaped_mask, mask);

        iters = _mm256_add_pd(iters, _mm256_andnot_pd(escaped_mask, one));

        simd_f128x4 zre_zim = simd_f128x4_mul(zre, zim);
        zim = simd_f128x4_add(simd_f128x4_mul2(zre_zim), cim);
        zre = simd_f128x4_add(simd_f128x4_abs(simd_f128x4_sub(zre2, zim2)), cre);
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
