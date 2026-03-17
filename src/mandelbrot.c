#include "mandelbrot.h"
#include <math.h>

/*
 * Returns the number of iterations before the point c escapes,
 * or MAX_ITERATIONS if it never does (considered inside the set).
 *
 * The Mandelbrot iteration is: z = z^2 + c, starting from z = 0.
 * A point escapes when |z| > ESCAPE_RADIUS.
 */
int mandelbrot_check(complex_t c) {
    complex_t z = {0.0, 0.0};
    int iterations = 0;

    /* Compare squared magnitudes to avoid calling sqrt() every iteration */
    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < MAX_ITERATIONS) {
        /* z = z^2 + c
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
