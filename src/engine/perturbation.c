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
    orbit->sa = malloc(sizeof(SACoeff) * max_iter);
    if (!orbit->zn || !orbit->sa) {
        if (orbit->zn) free(orbit->zn);
        if (orbit->sa) free(orbit->sa);
        free(orbit);
        return NULL;
    }

    precise_float z_re = 0.0;
    precise_float z_im = 0.0;

    // initial orbit element z0 = 0.0 + 0.0i
    orbit->zn[0].re = (double)z_re;
    orbit->zn[0].im = (double)z_im;

    // initial sa coefficients A=1, B=0, C=0
    double A_re = 1.0, A_im = 0.0;
    double B_re = 0.0, B_im = 0.0;
    double C_re = 0.0, C_im = 0.0;
    orbit->sa[0].A_re = A_re; orbit->sa[0].A_im = A_im;
    orbit->sa[0].B_re = B_re; orbit->sa[0].B_im = B_im;
    orbit->sa[0].C_re = C_re; orbit->sa[0].C_im = C_im;

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

        // compute SA coefficients before updating z_n
        // A_{n+1} = 2 z_n A_n + 1
        double next_A_re = 2.0 * ((double)z_re * A_re - (double)z_im * A_im) + 1.0;
        double next_A_im = 2.0 * ((double)z_re * A_im + (double)z_im * A_re);
        
        // B_{n+1} = 2 z_n B_n + A_n^2
        double A2_re = A_re * A_re - A_im * A_im;
        double A2_im = 2.0 * A_re * A_im;
        double next_B_re = 2.0 * ((double)z_re * B_re - (double)z_im * B_im) + A2_re;
        double next_B_im = 2.0 * ((double)z_re * B_im + (double)z_im * B_re) + A2_im;
        
        // C_{n+1} = 2 z_n C_n + 2 A_n B_n
        double AB_re = A_re * B_re - A_im * B_im;
        double AB_im = A_re * B_im + A_im * B_re;
        double next_C_re = 2.0 * ((double)z_re * C_re - (double)z_im * C_im) + 2.0 * AB_re;
        double next_C_im = 2.0 * ((double)z_re * C_im + (double)z_im * C_re) + 2.0 * AB_im;
        
        A_re = next_A_re; A_im = next_A_im;
        B_re = next_B_re; B_im = next_B_im;
        C_re = next_C_re; C_im = next_C_im;
        
        orbit->sa[i].A_re = A_re; orbit->sa[i].A_im = A_im;
        orbit->sa[i].B_re = B_re; orbit->sa[i].B_im = B_im;
        orbit->sa[i].C_re = C_re; orbit->sa[i].C_im = C_im;

        // complex formula: z_next = z^2 + c
        z_im = 2.0 * z_re * z_im + center_im;
        z_re = z_re2 - z_im2 + center_re;

        // store as double for gpu texture upload
        orbit->zn[i].re = (double)z_re;
        orbit->zn[i].im = (double)z_im;
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
        if (orbit->sa) {
            free(orbit->sa);
        }
        free(orbit);
    }
}
