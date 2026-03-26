#include "mandelbrot.h"
#include <math.h>

/*
 * Returns the number of iterations before the point c escapes,
 * or MAX_ITERATIONS if it never does (considered inside the set).
 *
 * The Mandelbrot iteration is: z = z^2 + c, starting from z = 0.
 * A point escapes when |z| > ESCAPE_RADIUS.
 */
double mandelbrot_check(complex_t c) {
    complex_t z = {0.0, 0.0};
    int iterations = 0;

    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < MAX_ITERATIONS) {
        double next_re = z.re * z.re - z.im * z.im + c.re;
        double next_im = 2.0 * z.re * z.im + c.im;
        z.re = next_re;
        z.im = next_im;

        double mag_sq = z.re * z.re + z.im * z.im;
        if (mag_sq > escape_radius_sq) {
            /* 
             * Smooth coloring formula: mu = iterations + 1 - log2(log(|z|))
             * Using mag_sq: mu = iterations + 2 - log2(log(mag_sq))
             */
            return (double)iterations + 2.0 - log2(log(mag_sq));
        }

        iterations++;
    }

    return (double)MAX_ITERATIONS;
}
