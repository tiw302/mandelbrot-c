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

#include <math.h>
#include <stdlib.h>

#include "bignum.h"
#include "mandelbrot_bignum.h"

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
    orbit->sa[0].A_re = A_re;
    orbit->sa[0].A_im = A_im;
    orbit->sa[0].B_re = B_re;
    orbit->sa[0].B_im = B_im;
    orbit->sa[0].C_re = C_re;
    orbit->sa[0].C_im = C_im;

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

        /* compute SA coefficients before updating z_n         * A_{n+1} = 2 z_n A_n + 1 */
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

        A_re = next_A_re;
        A_im = next_A_im;
        B_re = next_B_re;
        B_im = next_B_im;
        C_re = next_C_re;
        C_im = next_C_im;

        orbit->sa[i].A_re = A_re;
        orbit->sa[i].A_im = A_im;
        orbit->sa[i].B_re = B_re;
        orbit->sa[i].B_im = B_im;
        orbit->sa[i].C_re = C_re;
        orbit->sa[i].C_im = C_im;

        // z_next = z^2 + c
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

void perturbation_free(RefOrbit* orbit) {
    if (orbit) {
        if (orbit->zn) free(orbit->zn);
        if (orbit->sa) free(orbit->sa);
        free(orbit);
    }
}

RefPoint find_best_ref_point(precise_float center_re, precise_float center_im, precise_float zoom,
                             precise_float aspect, int max_iters, int grid_size) {
    const precise_float escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;

    RefPoint best = {center_re, center_im, 0.0f, 0.0f};
    double best_score = -1.0;

    for (int gy = 0; gy < grid_size; gy++) {
        float ny = (grid_size > 1) ? ((float)gy / (grid_size - 1) - 0.5f) : 0.0f;
        for (int gx = 0; gx < grid_size; gx++) {
            float nx = (grid_size > 1) ? ((float)gx / (grid_size - 1) - 0.5f) : 0.0f;
            precise_float c_re = center_re + (precise_float)nx * zoom * aspect;
            precise_float c_im = center_im + (precise_float)ny * zoom;

            int iters = 0;
            precise_float z_re = 0.0;
            precise_float z_im = 0.0;
            for (; iters < max_iters; iters++) {
                precise_float z_re2 = z_re * z_re;
                precise_float z_im2 = z_im * z_im;
                if (z_re2 + z_im2 > escape_radius_sq) break;
                z_im = 2.0 * z_re * z_im + c_im;
                z_re = z_re2 - z_im2 + c_re;
            }

            /* favor late-escaping points; break ties toward screen center */
            double dist_sq = (double)(nx * nx + ny * ny);
            double score = (double)iters - 1e-5 * dist_sq;
            if (score > best_score) {
                best_score = score;
                best.ref_re = c_re;
                best.ref_im = c_im;
                best.offset_x = nx;
                best.offset_y = ny;
            }
        }
    }

    return best;
}

/* computes the reference orbit using BigNum arithmetic.
 *
 * drop-in replacement for perturbation_compute() at zoom < BIGNUM_ZOOM_THRESHOLD,
 * where even long double / simd-f128 have lost all significant bits in the center coordinate.
 *
 * the center is loaded from a double — this is intentionally lossy for the coordinate itself,
 * but the orbit iteration is carried out at full 1024-bit BigNum precision, which is what matters.
 * sa coefficients are computed alongside at double precision (sa only needs relative accuracy). */
RefOrbit* perturbation_compute_bignum(double center_re, double center_im, int max_iter) {
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

    /* load center coordinate into BigNum — loses bits beyond double mantissa,
     * but subsequent iterations are carried out at full BigNum precision. */
    BigNum bn_re, bn_im;
    bn_from_double(&bn_re, center_re);
    bn_from_double(&bn_im, center_im);

    // scratch arrays for the BigNum orbit values (as double for gpu texture)
    double* orbit_re_buf = malloc(sizeof(double) * max_iter);
    double* orbit_im_buf = malloc(sizeof(double) * max_iter);
    if (!orbit_re_buf || !orbit_im_buf) {
        if (orbit_re_buf) free(orbit_re_buf);
        if (orbit_im_buf) free(orbit_im_buf);
        free(orbit->zn);
        free(orbit->sa);
        free(orbit);
        return NULL;
    }

    int len = mandelbrot_bignum_orbit(&bn_re, &bn_im, max_iter, orbit_re_buf, orbit_im_buf);

    /* copy BigNum orbit into RefOrbit and compute SA coefficients alongside.
     * SA only needs relative accuracy (it approximates delta skipping), so double is fine here. */
    double A_re = 1.0, A_im = 0.0;
    double B_re = 0.0, B_im = 0.0;
    double C_re = 0.0, C_im = 0.0;

    orbit->zn[0].re = orbit_re_buf[0];
    orbit->zn[0].im = orbit_im_buf[0];
    orbit->sa[0].A_re = A_re;
    orbit->sa[0].A_im = A_im;
    orbit->sa[0].B_re = B_re;
    orbit->sa[0].B_im = B_im;
    orbit->sa[0].C_re = C_re;
    orbit->sa[0].C_im = C_im;

    for (int i = 1; i < len; i++) {
        double z_re = orbit_re_buf[i - 1];
        double z_im = orbit_im_buf[i - 1];

        // A_{n+1} = 2 z_n A_n + 1
        double next_A_re = 2.0 * (z_re * A_re - z_im * A_im) + 1.0;
        double next_A_im = 2.0 * (z_re * A_im + z_im * A_re);

        // B_{n+1} = 2 z_n B_n + A_n^2
        double A2_re = A_re * A_re - A_im * A_im;
        double A2_im = 2.0 * A_re * A_im;
        double next_B_re = 2.0 * (z_re * B_re - z_im * B_im) + A2_re;
        double next_B_im = 2.0 * (z_re * B_im + z_im * B_re) + A2_im;

        // C_{n+1} = 2 z_n C_n + 2 A_n B_n
        double AB_re = A_re * B_re - A_im * B_im;
        double AB_im = A_re * B_im + A_im * B_re;
        double next_C_re = 2.0 * (z_re * C_re - z_im * C_im) + 2.0 * AB_re;
        double next_C_im = 2.0 * (z_re * C_im + z_im * C_re) + 2.0 * AB_im;

        A_re = next_A_re;
        A_im = next_A_im;
        B_re = next_B_re;
        B_im = next_B_im;
        C_re = next_C_re;
        C_im = next_C_im;

        orbit->zn[i].re = orbit_re_buf[i];
        orbit->zn[i].im = orbit_im_buf[i];
        orbit->sa[i].A_re = A_re;
        orbit->sa[i].A_im = A_im;
        orbit->sa[i].B_re = B_re;
        orbit->sa[i].B_im = B_im;
        orbit->sa[i].C_re = C_re;
        orbit->sa[i].C_im = C_im;
    }

    orbit->len = len;

    free(orbit_re_buf);
    free(orbit_im_buf);
    return orbit;
}
