/* bignum.h
 *
 * arbitrary-precision fixed-point integer arithmetic for infinite zoom.
 * provides a BigNum type that represents a signed real number as:
 *
 *   value = sign * (limbs[0] + limbs[1]*BASE + limbs[2]*BASE^2 + ...)
 *
 * representation:
 *   - limbs are stored least-significant first (limbs[0] is the lowest word)
 *   - the radix point is fixed after limbs[BN_INT_LIMBS - 1], meaning:
 *       limbs[0..BN_INT_LIMBS-1] = integer part
 *       limbs[BN_INT_LIMBS..BN_LIMBS-1] = fractional part
 *   - BASE = 2^32, so each limb holds one 32-bit digit
 *   - total precision = BN_LIMBS * 32 bits
 *
 * design philosophy:
 *   - pure c99, no external dependencies, no dynamic allocation
 *   - all operations take output as first argument (out-param style)
 *   - precision controlled by BN_LIMBS at compile time
 *   - intended for single-pixel reference orbit computation on the cpu
 */

#ifndef BIGNUM_H
#define BIGNUM_H

#include <stdint.h>
#include <string.h>

/* number of 32-bit limbs for the fractional part.
 * 32 limbs = 32 * 32 = 1024 bits of fractional precision.
 * sufficient for zoom depths past 10^300. */
#define BN_FRAC_LIMBS 32

/* integer part: 2 limbs cover [-2^64, 2^64) which is more than enough
 * since mandelbrot coordinates are always in roughly [-3, 3]. */
#define BN_INT_LIMBS 2

// total limb count (integer + fractional)
#define BN_LIMBS (BN_INT_LIMBS + BN_FRAC_LIMBS)

// each limb is an unsigned 32-bit word; arithmetic uses 64-bit intermediates
#define BN_BASE_BITS 32
#define BN_BASE_MASK 0xFFFFFFFFULL

typedef struct {
    uint32_t limbs[BN_LIMBS];  // little-endian limb array (limbs[0] = least significant)
    int sign;                  // +1 or -1 (zero is represented as sign=1, all limbs zero)
} BigNum;

/* -------------------------------------------------------------------------
 * construction
 * ---------------------------------------------------------------------- */

// sets bn to zero
void bn_zero(BigNum* bn);

// sets bn to the value of a standard 64-bit double
void bn_from_double(BigNum* bn, double val);

// converts bn back to a double (lossy — only returns the top 53 bits of precision)
double bn_to_double(const BigNum* bn);

/* -------------------------------------------------------------------------
 * arithmetic
 * ---------------------------------------------------------------------- */

// out = a + b
void bn_add(BigNum* out, const BigNum* a, const BigNum* b);

// out = a - b
void bn_sub(BigNum* out, const BigNum* a, const BigNum* b);

// out = a * b (truncated to BN_LIMBS precision — lower limbs are discarded)
void bn_mul(BigNum* out, const BigNum* a, const BigNum* b);

// out = a * 2 (optimization used in z_im = 2*z_re*z_im + c_im)
void bn_mul2(BigNum* out, const BigNum* a);

// returns 1 if |a| > |b|, -1 if |a| < |b|, 0 if equal (magnitude only, ignores sign)
int bn_cmp_mag(const BigNum* a, const BigNum* b);

/* -------------------------------------------------------------------------
 * helpers for the mandelbrot iteration loop
 * ---------------------------------------------------------------------- */

/* computes the squared magnitude |z|^2 = z_re^2 + z_im^2 and returns as double * used for escape
 * radius check (full bignum comparison not needed here) */
double bn_mag_sq_to_double(const BigNum* z_re, const BigNum* z_im);

#endif  // BIGNUM_H
