/* mandelbrot_bignum.h
 *
 * interface for arbitrary-precision mandelbrot computation using BigNum.
 * intended for reference orbit generation at extreme zoom depths.
 *
 * these functions are expensive and should only be called once per frame
 * for the reference point — all other pixels use fast perturbation delta arithmetic.
 */

#ifndef MANDELBROT_BIGNUM_H
#define MANDELBROT_BIGNUM_H

#include "bignum.h"

/* computes mandelbrot escape iteration count for a single point using BigNum precision.
 * returns smooth (fractional) iteration count, or (double)max_iterations if inside the set. */
double mandelbrot_check_bignum(const BigNum* c_re, const BigNum* c_im, int max_iterations);

/* computes a full reference orbit in BigNum precision for perturbation theory.
 *
 *   orbit_re_out, orbit_im_out: caller-allocated arrays of length max_iterations
 *
 * returns the orbit length (may be less than max_iterations if the reference escapes early).
 * the orbit values are stored as double for gpu texture upload. */
int mandelbrot_bignum_orbit(const BigNum* c_re, const BigNum* c_im, int max_iterations,
                            double* orbit_re_out, double* orbit_im_out);

#endif // MANDELBROT_BIGNUM_H
