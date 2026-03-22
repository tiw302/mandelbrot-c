#include "julia.h"

/*
 * Julia iteration is identical to Mandelbrot arithmetic -- the only difference
 * is that z starts at the given point instead of 0, and c is fixed for the
 * entire frame rather than varying per pixel.
 *
 * This separation keeps mandelbrot.c focused on the Mandelbrot set and makes
 * the distinction between the two sets explicit in the code.
 */
int julia_check(complex_t z, complex_t c) {
    int iterations = 0;
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < MAX_ITERATIONS) {
        /* z_new = z^2 + c
         * Re(z^2) = re^2 - im^2
         * Im(z^2) = 2 * re * im */
        double next_re = z.re * z.re - z.im * z.im + c.re;
        double next_im = 2.0 * z.re * z.im + c.im;
        z.re = next_re;
        z.im = next_im;

        if (z.re * z.re + z.im * z.im > escape_radius_sq)
            return iterations;

        iterations++;
    }

    return MAX_ITERATIONS;
}
