/* perturbation.c
 *
 * calculates the high-precision reference orbit for mandelbrot perturbation theory.
 * the reference orbit is computed once per frame on the cpu using high-precision
 * arithmetic (precise_float) and uploaded to the gpu to allow fast float-based
 * delta evaluation at deep zoom levels.
 */

#include "perturbation.h"
#include <stdlib.h>
#include <math.h>

/* computes the reference orbit starting at z0 = (0, 0) for the given center coordinates.
 * the orbit values are cast to single-precision float for shader consumption.
 * terminates early if the center point escapes the escape radius. */
RefOrbit* perturbation_compute(precise_float center_re, precise_float center_im, int max_iter) {
    RefOrbit* orbit = malloc(sizeof(RefOrbit));
    if (!orbit) return NULL;

    orbit->zn = malloc(sizeof(ComplexFloat) * max_iter);
    if (!orbit->zn) {
        free(orbit);
        return NULL;
    }

    precise_float z_re = 0.0;
    precise_float z_im = 0.0;

    // initial orbit element z0 = 0.0 + 0.0i
    orbit->zn[0].re = (float)z_re;
    orbit->zn[0].im = (float)z_im;

    int len = 1;
    const precise_float escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    // iteratively calculate reference orbit elements z_n
    for (int i = 1; i < max_iter; i++) {
        precise_float z_re2 = z_re * z_re;
        precise_float z_im2 = z_im * z_im;

        // stop computing reference orbit if it escapes the threshold
        if (z_re2 + z_im2 > escape_radius_sq) {
            break;
        }

        // complex formula: z_next = z^2 + c
        z_im = 2.0 * z_re * z_im + center_im;
        z_re = z_re2 - z_im2 + center_re;

        // store as float for gpu texture upload
        orbit->zn[i].re = (float)z_re;
        orbit->zn[i].im = (float)z_im;
        len++;
    }

    orbit->len = len;
    return orbit;
}

// safely deallocates orbit structures
void perturbation_free(RefOrbit* orbit) {
    if (orbit) {
        if (orbit->zn) {
            free(orbit->zn);
        }
        free(orbit);
    }
}
