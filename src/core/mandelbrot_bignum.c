/* mandelbrot_bignum.c
 *
 * arbitrary-precision mandelbrot iteration using the BigNum engine.
 * intended for single reference point computation (perturbation theory source).
 *
 * usage in the pipeline:
 *   1. at extreme zoom depths (past ~10^300), standard doubles and even
 *      double-double (simd-f128) lose all precision.
 *   2. this kernel computes a single center reference orbit at full BigNum
 *      precision (1024 fractional bits by default).
 *   3. all other screen pixels are still computed using fast perturbation
 *      delta arithmetic with 32-bit floats on the gpu.
 *
 * formula: z_{n+1} = z_n^2 + c
 *   in components:
 *     z_re_next = z_re^2 - z_im^2 + c_re
 *     z_im_next = 2 * z_re * z_im  + c_im
 *
 * performance:
 *   this is NOT called per pixel. one invocation per frame (for the reference point).
 *   at BN_FRAC_LIMBS=32, one iteration costs ~32^2 = 1024 limb multiplications.
 *   for 10000 iterations that is ~10M multiplications — acceptable for a once-per-frame call.
 */

#include "mandelbrot_bignum.h"
#include "bignum.h"
#include <stdlib.h>
#include <string.h>

/* computes the mandelbrot escape iteration count for a single coordinate (c_re, c_im)
 * using arbitrary-precision BigNum arithmetic.
 *
 * the center c coordinates must already be loaded into BigNum format by the caller
 * (via bn_from_double or manually for extreme precision).
 *
 * returns fractional iteration count (smooth coloring formula applied at end),
 * or (double)max_iterations if the point is inside the set. */
double mandelbrot_check_bignum(const BigNum* c_re, const BigNum* c_im, int max_iterations) {
    BigNum z_re, z_im;
    bn_zero(&z_re);
    bn_zero(&z_im);

    // temps for intermediate calculations to avoid aliasing issues
    BigNum z_re2, z_im2, z_re_zim, z_re_next, z_im_next, z_re_zim2;
    const double escape_radius_sq = 10.0 * 10.0; // matches ESCAPE_RADIUS in config.h

    int iterations = 0;

    while (iterations < max_iterations) {
        // z_re^2 and z_im^2
        bn_mul(&z_re2, &z_re, &z_re);
        bn_mul(&z_im2, &z_im, &z_im);

        /* escape check: convert |z|^2 to double for threshold test.
         * this is intentionally lossy — we only need to know if |z| > 10,
         * so the top few bits of precision are more than sufficient. */
        double mag_sq = bn_mag_sq_to_double(&z_re, &z_im);
        if (mag_sq > escape_radius_sq) {
            /* smooth coloring: same formula as the standard scalar path.
             * guards against log(0) using fmax just in case. */
            double mag_sq_safe = mag_sq > 2.0 ? mag_sq : 2.0;
            double smooth = (double)iterations + 2.0 - __builtin_log2(__builtin_log(mag_sq_safe));
            return smooth;
        }

        // z_re_next = z_re^2 - z_im^2 + c_re
        bn_sub(&z_re_next, &z_re2, &z_im2);
        bn_add(&z_re_next, &z_re_next, c_re);

        // z_im_next = 2 * z_re * z_im + c_im
        bn_mul(&z_re_zim, &z_re, &z_im);
        bn_mul2(&z_re_zim2, &z_re_zim);  // 2 * z_re * z_im (cheaper than a second bn_mul)
        bn_add(&z_im_next, &z_re_zim2, c_im);

        z_re = z_re_next;
        z_im = z_im_next;
        iterations++;
    }

    // point is inside the set
    return (double)max_iterations;
}

/* computes a reference orbit in BigNum precision for use with perturbation theory.
 *
 * fills orbit_out[] with the z_n values converted back to double pairs (re, im).
 * the caller is responsible for allocating orbit_out with at least max_iter elements.
 *
 * returns the actual orbit length (may be shorter if the reference point escapes early).
 *
 * note: the orbit is stored as double for gpu texture upload, same as perturbation_compute().
 * the bignum precision is only used during the iteration itself. */
int mandelbrot_bignum_orbit(const BigNum* c_re, const BigNum* c_im,
                            int max_iterations,
                            double* orbit_re_out, double* orbit_im_out) {
    BigNum z_re, z_im;
    bn_zero(&z_re);
    bn_zero(&z_im);

    BigNum z_re2, z_im2, z_re_zim, z_re_next, z_im_next, z_re_zim2;
    const double escape_radius_sq = 10.0 * 10.0;

    // z_0 = 0.0 + 0.0i
    orbit_re_out[0] = 0.0;
    orbit_im_out[0] = 0.0;

    int len = 1;
    for (int i = 1; i < max_iterations; i++) {
        bn_mul(&z_re2, &z_re, &z_re);
        bn_mul(&z_im2, &z_im, &z_im);

        /* escape check using double projection.
         * once the reference orbit escapes, delta arithmetic can't reference it anymore.
         * the caller should fall back to direct computation beyond this orbit length. */
        double mag_sq = bn_mag_sq_to_double(&z_re, &z_im);
        if (mag_sq > escape_radius_sq) break;

        // z_re_next = z_re^2 - z_im^2 + c_re
        bn_sub(&z_re_next, &z_re2, &z_im2);
        bn_add(&z_re_next, &z_re_next, c_re);

        // z_im_next = 2 * z_re * z_im + c_im
        bn_mul(&z_re_zim, &z_re, &z_im);
        bn_mul2(&z_re_zim2, &z_re_zim);
        bn_add(&z_im_next, &z_re_zim2, c_im);

        z_re = z_re_next;
        z_im = z_im_next;

        // store as double for gpu texture consumption
        orbit_re_out[i] = bn_to_double(&z_re);
        orbit_im_out[i] = bn_to_double(&z_im);
        len++;
    }

    return len;
}
