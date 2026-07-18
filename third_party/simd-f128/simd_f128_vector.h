// updated 2026-06-12
// spdx-license-identifier: mit
// copyright (c) 2026 jirawat siripuk

#ifndef SIMD_F128_VECTOR_H
#define SIMD_F128_VECTOR_H

#include "simd_f128.h"
#include "simd_f128_utils.h"

// ██████  ███████ ██████  ███████ ███████  ██████ ████████ 
// ██   ██ ██      ██   ██ ██      ██      ██         ██    
// ██████  █████   ██████  █████   █████   ██         ██    
// ██      ██      ██   ██ ██      ██      ██         ██    
// ██      ███████ ██   ██ ███████ ███████  ██████    ██    
//
// >>vectorized api (simd_f128x4 for avx2)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SIMD_F128_USE_AVX2)

/*
 * simd_f128x4 processes four double-doubles simultaneously.
 * this is the pinnacle of parallel performance on x86_64.
 *
 * warning: pure vectorized operations here may propagate nan and inf
 * differently than the scalar or standard inline functions, since
 * branches are omitted in favor of blend masks for performance.
 */
typedef struct {
    __m256d hi;
    __m256d lo;
} simd_f128x4;

// initialize a vectorized simd_f128x4 struct from four individual doubles
// hi registers are loaded with the values, lo registers are initialized to zero
SIMD_F128_INLINE simd_f128x4 simd_f128x4_from_doubles(double d0, double d1, double d2, double d3) {
    simd_f128x4 res;
    res.hi = _mm256_setr_pd(d0, d1, d2, d3);
    res.lo = _mm256_setzero_pd();
    return res;
}

// perform parallel addition of four double-double values using knuth's two-sum
SIMD_F128_INLINE simd_f128x4 simd_f128x4_add(simd_f128x4 a, simd_f128x4 b) {
    __m256d s = _mm256_add_pd(a.hi, b.hi);
    __m256d diff = _mm256_sub_pd(s, s);
    __m256d inf_mask = _mm256_cmp_pd(diff, diff, _CMP_UNORD_Q);
    __m256d v = _mm256_sub_pd(s, a.hi);
    __m256d e = _mm256_add_pd(_mm256_sub_pd(a.hi, _mm256_sub_pd(s, v)), _mm256_sub_pd(b.hi, v));
    __m256d t = _mm256_add_pd(_mm256_add_pd(a.lo, b.lo), e);
    
    simd_f128x4 res;
    res.hi = _mm256_add_pd(s, t);
    res.lo = _mm256_sub_pd(t, _mm256_sub_pd(res.hi, s));
    // if s is infinite or nan, set lo component to zero to prevent invalid values
    res.lo = _mm256_blendv_pd(res.lo, _mm256_setzero_pd(), inf_mask);
    return res;
}

// perform parallel multiplication of four double-double values
SIMD_F128_INLINE simd_f128x4 simd_f128x4_mul(simd_f128x4 a, simd_f128x4 b) {
    __m256d hi_prod = _mm256_mul_pd(a.hi, b.hi);
    __m256d diff = _mm256_sub_pd(hi_prod, hi_prod);
    __m256d inf_mask = _mm256_cmp_pd(diff, diff, _CMP_UNORD_Q);
    
    // exact multiplication error estimation:
    // uses hardware fma if supported, otherwise falls back to a vector split method
#if defined(__FMA__)
    __m256d err = _mm256_fmsub_pd(a.hi, b.hi, hi_prod);
#else
    // vector split using dekker's constant (2^27 + 1)
    __m256d c = _mm256_set1_pd(134217729.0);
    __m256d up = _mm256_mul_pd(a.hi, c);
    __m256d vp = _mm256_mul_pd(b.hi, c);
    __m256d u1 = _mm256_sub_pd(up, _mm256_sub_pd(up, a.hi));
    __m256d u2 = _mm256_sub_pd(a.hi, u1);
    __m256d v1 = _mm256_sub_pd(vp, _mm256_sub_pd(vp, b.hi));
    __m256d v2 = _mm256_sub_pd(b.hi, v1);
    __m256d err = _mm256_add_pd(_mm256_add_pd(_mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(u1, v1), hi_prod), _mm256_mul_pd(u1, v2)), _mm256_mul_pd(u2, v1)), _mm256_mul_pd(u2, v2));
#endif

    // accumulate cross-terms and the exact multiplication roundoff error
    __m256d lo_prod = _mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(a.hi, b.lo), _mm256_mul_pd(a.lo, b.hi)), err);
    
    simd_f128x4 res;
    res.hi = _mm256_add_pd(hi_prod, lo_prod);
    res.lo = _mm256_sub_pd(lo_prod, _mm256_sub_pd(res.hi, hi_prod));
    // if hi_prod is infinite or nan, set lo component to zero and restore hi_prod to prevent nan propagation
    res.hi = _mm256_blendv_pd(res.hi, hi_prod, inf_mask);
    res.lo = _mm256_blendv_pd(res.lo, _mm256_setzero_pd(), inf_mask);
    return res;
}

// negate all four double-double elements in parallel
SIMD_F128_INLINE simd_f128x4 simd_f128x4_neg(simd_f128x4 a) {
    simd_f128x4 res;
    res.hi = _mm256_xor_pd(a.hi, _mm256_set1_pd(-0.0));
    res.lo = _mm256_xor_pd(a.lo, _mm256_set1_pd(-0.0));
    return res;
}

// subtract four double-double elements in parallel
SIMD_F128_INLINE simd_f128x4 simd_f128x4_sub(simd_f128x4 a, simd_f128x4 b) {
    return simd_f128x4_add(a, simd_f128x4_neg(b));
}

// multiply four double-double elements by two in parallel
SIMD_F128_INLINE simd_f128x4 simd_f128x4_mul2(simd_f128x4 a) {
    simd_f128x4 res;
    res.hi = _mm256_add_pd(a.hi, a.hi);
    res.lo = _mm256_add_pd(a.lo, a.lo);
    return res;
}

// square all four double-double elements in parallel (a * a)
SIMD_F128_INLINE simd_f128x4 simd_f128x4_sqr(simd_f128x4 a) {
    return simd_f128x4_mul(a, a);
}

// compute absolute values of all four double-double elements in parallel
SIMD_F128_INLINE simd_f128x4 simd_f128x4_abs(simd_f128x4 a) {
    __m256d zero = _mm256_setzero_pd();
    __m256d sign_hi = _mm256_and_pd(a.hi, _mm256_set1_pd(-0.0));
    __m256d eq_zero = _mm256_cmp_pd(a.hi, zero, _CMP_EQ_OQ);
    __m256d abs_lo = _mm256_andnot_pd(_mm256_set1_pd(-0.0), a.lo);
    
    __m256d normal_hi = _mm256_xor_pd(a.hi, sign_hi);
    __m256d normal_lo = _mm256_xor_pd(a.lo, sign_hi);
    
    simd_f128x4 res;
    res.hi = _mm256_blendv_pd(normal_hi, zero, eq_zero);
    res.lo = _mm256_blendv_pd(normal_lo, abs_lo, eq_zero);
    return res;
}

// perform parallel division of four double-double elements using avx2 vector math
SIMD_F128_INLINE simd_f128x4 simd_f128x4_div(simd_f128x4 a, simd_f128x4 b) {
    __m256d q1 = _mm256_div_pd(a.hi, b.hi);
    __m256d p1 = _mm256_mul_pd(q1, b.hi);
    
#if defined(__FMA__)
    __m256d p2 = _mm256_fmadd_pd(q1, b.lo, _mm256_fmsub_pd(q1, b.hi, p1));
#else
    // dekker's split fallback for vector FMA if __FMA__ is not defined
    __m256d c = _mm256_set1_pd(134217729.0);
    __m256d up = _mm256_mul_pd(q1, c);
    __m256d vp = _mm256_mul_pd(b.hi, c);
    __m256d u1 = _mm256_sub_pd(up, _mm256_sub_pd(up, q1));
    __m256d u2 = _mm256_sub_pd(q1, u1);
    __m256d v1 = _mm256_sub_pd(vp, _mm256_sub_pd(vp, b.hi));
    __m256d v2 = _mm256_sub_pd(b.hi, v1);
    __m256d mul_err = _mm256_add_pd(_mm256_add_pd(_mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(u1, v1), p1), _mm256_mul_pd(u1, v2)), _mm256_mul_pd(u2, v1)), _mm256_mul_pd(u2, v2));
    __m256d p2 = _mm256_add_pd(_mm256_mul_pd(q1, b.lo), mul_err);
#endif

    __m256d s = _mm256_sub_pd(a.hi, p1);
    __m256d v = _mm256_sub_pd(s, a.hi);
    __m256d e = _mm256_add_pd(
        _mm256_sub_pd(a.hi, _mm256_sub_pd(s, v)),
        _mm256_sub_pd(_mm256_sub_pd(_mm256_setzero_pd(), p1), v)
    );
    __m256d t = _mm256_add_pd(_mm256_sub_pd(a.lo, p2), e);

    __m256d rh = _mm256_add_pd(s, t);
    __m256d rl = _mm256_sub_pd(t, _mm256_sub_pd(rh, s));
    __m256d q2 = _mm256_div_pd(rh, b.hi);

    simd_f128x4 res;
    res.hi = _mm256_add_pd(q1, q2);
    res.lo = _mm256_add_pd(_mm256_sub_pd(q2, _mm256_sub_pd(res.hi, q1)), _mm256_div_pd(rl, b.hi));

    // check if q1 is Inf/NaN
    __m256d q1_invalid = _mm256_cmp_pd(_mm256_sub_pd(q1, q1), _mm256_setzero_pd(), _CMP_UNORD_Q);
    res.hi = _mm256_blendv_pd(res.hi, q1, q1_invalid);
    res.lo = _mm256_blendv_pd(res.lo, _mm256_setzero_pd(), q1_invalid);

    // prevent nan/inf propagation by zeroing lo component on overflow/underflow
    __m256d diff = _mm256_sub_pd(res.hi, res.hi);
    __m256d inf_mask = _mm256_cmp_pd(diff, diff, _CMP_UNORD_Q);
    res.lo = _mm256_blendv_pd(res.lo, _mm256_setzero_pd(), inf_mask);

    return res;
}

// perform parallel square root of four double-double elements using avx2 vector math
SIMD_F128_INLINE simd_f128x4 simd_f128x4_sqrt(simd_f128x4 a) {
    __m256d zero = _mm256_setzero_pd();
    __m256d one = _mm256_set1_pd(1.0);
    
    // initial hardware guess for 1/sqrt(xhi)
    __m256d y = _mm256_div_pd(one, _mm256_sqrt_pd(a.hi));
    __m256d z = _mm256_mul_pd(a.hi, y);

#if defined(__FMA__)
    __m256d zlo = _mm256_fmadd_pd(a.lo, y, _mm256_fmsub_pd(a.hi, y, z));
#else
    __m256d c = _mm256_set1_pd(134217729.0);
    __m256d up = _mm256_mul_pd(a.hi, c);
    __m256d vp = _mm256_mul_pd(y, c);
    __m256d u1 = _mm256_sub_pd(up, _mm256_sub_pd(up, a.hi));
    __m256d u2 = _mm256_sub_pd(a.hi, u1);
    __m256d v1 = _mm256_sub_pd(vp, _mm256_sub_pd(vp, y));
    __m256d v2 = _mm256_sub_pd(y, v1);
    __m256d mul_err = _mm256_add_pd(_mm256_add_pd(_mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(u1, v1), z), _mm256_mul_pd(u1, v2)), _mm256_mul_pd(u2, v1)), _mm256_mul_pd(u2, v2));
    __m256d zlo = _mm256_add_pd(_mm256_mul_pd(a.lo, y), mul_err);
#endif

    __m256d est = _mm256_mul_pd(z, z);

#if defined(__FMA__)
    __m256d estlo = _mm256_fmadd_pd(_mm256_mul_pd(_mm256_set1_pd(2.0), z), zlo, _mm256_fmsub_pd(z, z, est));
#else
    __m256d up2 = _mm256_mul_pd(z, c);
    __m256d u1_2 = _mm256_sub_pd(up2, _mm256_sub_pd(up2, z));
    __m256d u2_2 = _mm256_sub_pd(z, u1_2);
    __m256d mul_err2 = _mm256_add_pd(_mm256_add_pd(_mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(u1_2, u1_2), est), _mm256_mul_pd(u1_2, u2_2)), _mm256_mul_pd(u2_2, u1_2)), _mm256_mul_pd(u2_2, u2_2));
    __m256d estlo = _mm256_add_pd(_mm256_mul_pd(_mm256_mul_pd(_mm256_set1_pd(2.0), z), zlo), mul_err2);
#endif

    // refine initial guess and compute error of z^2 vs x
    __m256d err = _mm256_add_pd(_mm256_sub_pd(_mm256_sub_pd(a.hi, est), estlo), a.lo);
    __m256d half_err_y = _mm256_mul_pd(_mm256_set1_pd(0.5), _mm256_mul_pd(err, y));

    __m256d final_hi = _mm256_add_pd(z, half_err_y);
    __m256d final_lo = _mm256_sub_pd(half_err_y, _mm256_sub_pd(final_hi, z));

    // handle bounds and special values
    __m256d lt_zero = _mm256_cmp_pd(a.hi, zero, _CMP_LT_OS);
    __m256d eq_zero = _mm256_cmp_pd(a.hi, zero, _CMP_EQ_OQ);
    __m256d lo_lt_zero = _mm256_cmp_pd(a.lo, zero, _CMP_LT_OS);
    __m256d invalid_mask = _mm256_or_pd(lt_zero, _mm256_and_pd(eq_zero, lo_lt_zero));
    
    __m256d is_inf = _mm256_cmp_pd(a.hi, _mm256_set1_pd(INFINITY), _CMP_EQ_OQ);
    __m256d pass_mask = _mm256_or_pd(eq_zero, is_inf);

    simd_f128x4 res;
    res.hi = _mm256_blendv_pd(final_hi, a.hi, pass_mask);
    res.lo = _mm256_blendv_pd(final_lo, a.lo, pass_mask);
    res.hi = _mm256_blendv_pd(res.hi, _mm256_set1_pd(NAN), invalid_mask);
    res.lo = _mm256_blendv_pd(res.lo, zero, invalid_mask);

    return res;
}

#else // !defined(SIMD_F128_USE_AVX2)

/*
 * fallback implementation of simd_f128x4 for non-AVX2 platforms
 */
typedef struct {
    simd_f128 val[4];
} simd_f128x4;

SIMD_F128_INLINE simd_f128x4 simd_f128x4_from_doubles(double d0, double d1, double d2, double d3) {
    simd_f128x4 res;
    res.val[0] = simd_f128_from_double(d0);
    res.val[1] = simd_f128_from_double(d1);
    res.val[2] = simd_f128_from_double(d2);
    res.val[3] = simd_f128_from_double(d3);
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_add(simd_f128x4 a, simd_f128x4 b) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_add(a.val[i], b.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_mul(simd_f128x4 a, simd_f128x4 b) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_mul(a.val[i], b.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_neg(simd_f128x4 a) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_neg(a.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_sub(simd_f128x4 a, simd_f128x4 b) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_sub(a.val[i], b.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_mul2(simd_f128x4 a) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_add(a.val[i], a.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_sqr(simd_f128x4 a) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_mul(a.val[i], a.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_abs(simd_f128x4 a) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_abs(a.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_div(simd_f128x4 a, simd_f128x4 b) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_div(a.val[i], b.val[i]);
    }
    return res;
}

SIMD_F128_INLINE simd_f128x4 simd_f128x4_sqrt(simd_f128x4 a) {
    simd_f128x4 res;
    for (int i = 0; i < 4; i++) {
        res.val[i] = simd_f128_sqrt(a.val[i]);
    }
    return res;
}

#endif // simd_f128_use_avx2

#ifdef __cplusplus
}
#endif

#endif // simd_f128_vector_h
