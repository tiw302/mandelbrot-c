/* perturbation.c
 *
 * reference orbit computation for perturbation theory rendering.
 * calculates high-precision paths on the cpu for deep zoom reference.
 *
 * process:
 *   - determines the center reference point and calculates its orbit on cpu
 *   - detects orbit period and calculates reference coordinates to a list
 *   - uploads reference orbit path for the gpu shader to perform delta-arithmetic
 *
 * theory:
 *   when zooming extremely deep (past 10^14), 64-bit double precision floats suffer
 *   from pixelation. perturbation theory solves this: instead of calculating absolute
 *   coordinates z_{n+1} = z_n^2 + c in high-precision, we compute a single high-precision
 *   reference orbit w_n on the cpu. then, all other screen pixels are computed as
 *   low-precision deltas z_n = w_n + d_n. the iteration formula for delta is:
 *     d_{n+1} = 2*w_n*d_n + d_n^2 + delta_c
 *   which can be computed using fast 32-bit floats on the gpu.
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

    /* iteratively calculate reference orbit elements z_n.
     * reference orbit must use high-precision variables (double-precision or double-single)
     * because any error in the reference orbit is propagated to all delta calculations. */
    for (int i = 1; i < max_iter; i++) {
        precise_float z_re2 = z_re * z_re;
        precise_float z_im2 = z_im * z_im;

        /* stop computing reference orbit if it escapes the threshold.
         * once the reference orbit escapes, delta calculations can no longer be referenced. */
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
