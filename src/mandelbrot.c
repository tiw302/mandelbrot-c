#include "mandelbrot.h"
#include <math.h>

double mandelbrot_check(complex_t c) {
    complex_t z = {0.0, 0.0};
    int iterations = 0;

    const double escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    while (iterations < MAX_ITERATIONS) {
        /* z_new = z^2 + c */
        double next_re = z.re * z.re - z.im * z.im + c.re;
        double next_im = 2.0 * z.re * z.im + c.im;
        z.re = next_re;
        z.im = next_im;

        double mag_sq = z.re * z.re + z.im * z.im;
        if (mag_sq > escape_radius_sq) {
            /* Renormalized iteration count for smooth coloring */
            return (double)iterations + 2.0 - log2(log(mag_sq));
        }

        iterations++;
    }

    return (double)MAX_ITERATIONS;
}
