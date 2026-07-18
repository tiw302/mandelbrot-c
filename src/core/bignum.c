/* bignum.c
 *
 * arbitrary-precision fixed-point arithmetic engine.
 * implements the BigNum type defined in bignum.h for use in deep zoom rendering.
 *
 * number format (little-endian limb array, LSB at index 0):
 *   - limbs[0..BN_FRAC_LIMBS-1] = fractional part (limbs[0] = least significant fraction word)
 *   - limbs[BN_FRAC_LIMBS..BN_LIMBS-1] = integer part (limbs[BN_LIMBS-1] = most significant)
 *   - the binary point sits between limbs[BN_FRAC_LIMBS-1] and limbs[BN_FRAC_LIMBS]
 *
 * example for BN_INT_LIMBS=2, BN_FRAC_LIMBS=4 (total 6 limbs):
 *   index:  0      1      2      3      4      5
 *           frac3  frac2  frac1  frac0  int0   int1
 *           (lsb)                       ^radix (msb)
 *
 * storing fractions at low indices ensures that add_limbs()/sub_limbs()
 * naturally carry from fractional words up into integer words.
 *
 * sign-magnitude representation is used instead of two's complement because
 * it makes mul/div significantly simpler when extending precision.
 *
 * performance note:
 *   this is intended for single reference point computation only (perturbation theory).
 *   it is NOT used per-pixel — that would be far too slow.
 *   delta arithmetic using fast floats handles all other pixels.
 */

#include "bignum.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * internal helpers
 * ---------------------------------------------------------------------- */

/* adds two limb arrays of length BN_LIMBS without considering sign.
 * carry propagates from index 0 (lsb) up to BN_LIMBS-1 (msb). */
static uint32_t add_limbs(uint32_t* out, const uint32_t* a, const uint32_t* b) {
    uint64_t carry = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        uint64_t sum = (uint64_t)a[i] + b[i] + carry;
        out[i] = (uint32_t)(sum & BN_BASE_MASK);
        carry = sum >> BN_BASE_BITS;
    }
    return (uint32_t)carry;
}

/* subtracts b from a (|a| >= |b| required by caller).
 * borrow propagates from index 0 up. */
static uint32_t sub_limbs(uint32_t* out, const uint32_t* a, const uint32_t* b) {
    uint64_t borrow = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        uint64_t diff = (uint64_t)a[i] - b[i] - borrow;
        out[i] = (uint32_t)(diff & BN_BASE_MASK);
        // borrow is set if the subtraction underflowed (bit 32 is set in diff)
        borrow = (diff >> 63) & 1;
    }
    return (uint32_t)borrow;
}

/* -------------------------------------------------------------------------
 * construction
 * ---------------------------------------------------------------------- */

void bn_zero(BigNum* bn) {
    memset(bn->limbs, 0, sizeof(bn->limbs));
    bn->sign = 1;
}

void bn_from_double(BigNum* bn, double val) {
    bn_zero(bn);

    // extract sign and work with absolute value
    if (val < 0.0) {
        bn->sign = -1;
        val = -val;
    } else {
        bn->sign = 1;
    }

    double remaining = val;

    /* place the integer part into the high limbs.
     * limbs[BN_FRAC_LIMBS] has weight 2^0 (just above radix),
     * limbs[BN_FRAC_LIMBS + 1] has weight 2^32, etc.
     * we process from the most significant integer limb down. */
    for (int i = BN_INT_LIMBS - 1; i >= 0; i--) {
        double word_scale = 1.0;
        for (int p = 0; p < i; p++) {
            word_scale *= 4294967296.0;  // 2^32
        }
        uint32_t word = (uint32_t)(remaining / word_scale);
        bn->limbs[BN_FRAC_LIMBS + i] = word;
        remaining -= word * word_scale;
    }

    /* place the fractional part into the low limbs.
     * limbs[BN_FRAC_LIMBS - 1] holds the most significant fraction word
     * (value 2^-32 per unit), limbs[0] holds the least significant. */
    for (int i = BN_FRAC_LIMBS - 1; i >= 0; i--) {
        remaining *= 4294967296.0;  // shift left by 32 bits
        uint32_t word = (remaining >= 4294967296.0) ? 0xFFFFFFFFu : (uint32_t)remaining;
        bn->limbs[i] = word;
        remaining -= (double)word;
    }
}

double bn_to_double(const BigNum* bn) {
    /* reconstruct double from limbs.
     * we accumulate from the most significant limb downward.
     * only the top few limbs are significant in a 53-bit double mantissa. */
    double result = 0.0;

    /* integer part: limbs[BN_FRAC_LIMBS..BN_LIMBS-1]     * limbs[BN_FRAC_LIMBS] has weight 1.0,
     * limbs[BN_FRAC_LIMBS+1] has weight 2^32, etc. */
    double int_weight = 1.0;
    for (int i = BN_FRAC_LIMBS; i < BN_LIMBS; i++) {
        result += (double)bn->limbs[i] * int_weight;
        int_weight *= 4294967296.0;
    }

    // fractional part: limbs[BN_FRAC_LIMBS-1] has weight 2^-32, limbs[0] has weight
    // 2^(-32*BN_FRAC_LIMBS)
    double frac_weight = 1.0 / 4294967296.0;
    for (int i = BN_FRAC_LIMBS - 1; i >= 0; i--) {
        result += (double)bn->limbs[i] * frac_weight;
        frac_weight /= 4294967296.0;
        // stop once weight is below double precision — no more accuracy to gain
        if (frac_weight == 0.0) break;
    }

    return bn->sign * result;
}

/* -------------------------------------------------------------------------
 * magnitude comparison (ignores sign)
 * ---------------------------------------------------------------------- */

int bn_cmp_mag(const BigNum* a, const BigNum* b) {
    // compare from most significant limb (highest index) downward
    for (int i = BN_LIMBS - 1; i >= 0; i--) {
        if (a->limbs[i] > b->limbs[i]) return 1;
        if (a->limbs[i] < b->limbs[i]) return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * addition
 * ---------------------------------------------------------------------- */

void bn_add(BigNum* out, const BigNum* a, const BigNum* b) {
    if (a->sign == b->sign) {
        // same sign: magnitude add, keep sign
        add_limbs(out->limbs, a->limbs, b->limbs);
        out->sign = a->sign;
        return;
    }

    // opposite signs: magnitude subtract, result sign follows the larger magnitude
    int cmp = bn_cmp_mag(a, b);
    if (cmp == 0) {
        // |a| == |b| -> result is zero
        bn_zero(out);
        return;
    }

    if (cmp > 0) {
        // |a| > |b|, so result = |a| - |b|, sign = a->sign
        sub_limbs(out->limbs, a->limbs, b->limbs);
        out->sign = a->sign;
    } else {
        // |b| > |a|, so result = |b| - |a|, sign = b->sign
        sub_limbs(out->limbs, b->limbs, a->limbs);
        out->sign = b->sign;
    }
}

/* -------------------------------------------------------------------------
 * subtraction
 * ---------------------------------------------------------------------- */

void bn_sub(BigNum* out, const BigNum* a, const BigNum* b) {
    // a - b == a + (-b)
    BigNum neg_b = *b;
    neg_b.sign = -b->sign;
    bn_add(out, a, &neg_b);
}

/* -------------------------------------------------------------------------
 * multiplication
 * ---------------------------------------------------------------------- */

void bn_mul(BigNum* out, const BigNum* a, const BigNum* b) {
    /* long multiplication in base 2^32.
     *
     * the product of two BN_LIMBS-limb numbers is 2*BN_LIMBS limbs wide.
     * layout: product[0] is lsb, product[2*BN_LIMBS-1] is msb.
     *
     * in our fixed-point layout, the radix point is at index BN_FRAC_LIMBS from the lsb.
     * so the product's radix point is at 2 * BN_FRAC_LIMBS from the lsb.
     * we discard the bottom BN_FRAC_LIMBS limbs (sub-precision) and keep BN_LIMBS
     * limbs starting from index BN_FRAC_LIMBS. */

    // temp product array is 2 * BN_LIMBS limbs wide
    uint64_t product[2 * BN_LIMBS];
    memset(product, 0, sizeof(product));

    // O(n^2) schoolbook multiplication — sufficient for BN_LIMBS=34
    for (int i = 0; i < BN_LIMBS; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < BN_LIMBS; j++) {
            uint64_t cur = product[i + j] + (uint64_t)a->limbs[i] * b->limbs[j] + carry;
            product[i + j] = cur & BN_BASE_MASK;
            carry = cur >> BN_BASE_BITS;
        }
        product[i + BN_LIMBS] += carry;
    }

    /* copy BN_LIMBS words from position BN_FRAC_LIMBS in the product.
     * this is the radix-adjusted result: discards sub-precision fraction
     * and keeps the correctly scaled output. */
    for (int i = 0; i < BN_LIMBS; i++) {
        out->limbs[i] = (uint32_t)(product[i + BN_FRAC_LIMBS] & BN_BASE_MASK);
    }

    // sign follows standard sign rule: same -> positive, different -> negative
    out->sign = (a->sign == b->sign) ? 1 : -1;

    // zero result should always have positive sign (canonical form)
    int is_zero = 1;
    for (int i = 0; i < BN_LIMBS; i++) {
        if (out->limbs[i] != 0) {
            is_zero = 0;
            break;
        }
    }
    if (is_zero) out->sign = 1;
}

/* -------------------------------------------------------------------------
 * multiply by 2 (optimized bit-shift instead of full mul)
 * ---------------------------------------------------------------------- */

void bn_mul2(BigNum* out, const BigNum* a) {
    /* shift left by 1 bit across all limbs, from lsb to msb.
     * this is the fast path for z_im = 2 * z_re * z_im (used every iteration). */
    uint32_t carry = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        uint64_t shifted = ((uint64_t)a->limbs[i] << 1) | carry;
        out->limbs[i] = (uint32_t)(shifted & BN_BASE_MASK);
        carry = (uint32_t)(shifted >> BN_BASE_BITS);
    }
    out->sign = a->sign;
}

/* -------------------------------------------------------------------------
 * escape radius check helper
 * ---------------------------------------------------------------------- */

double bn_mag_sq_to_double(const BigNum* z_re, const BigNum* z_im) {
    /* returns |z|^2 = z_re^2 + z_im^2 as a double.
     * used only for the escape radius check in the iteration loop.
     * converting to double here is safe because we only need to know
     * whether the magnitude exceeds a threshold (~100 for radius=10),
     * not the full-precision value. */
    double re = bn_to_double(z_re);
    double im = bn_to_double(z_im);
    return re * re + im * im;
}
