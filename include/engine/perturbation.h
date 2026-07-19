/* perturbation.h
 *
 * interface for calculating and managing the high-precision reference orbit
 * on the cpu. this reference orbit is uploaded to the gpu as a texture for
 * delta calculations in perturbation rendering mode.
 */

#ifndef PERTURBATION_H
#define PERTURBATION_H

#include "config.h"

/* orbit point stored as double — the comment in the header says "single-precision"
 * but the fields are double. the name ComplexFloat is kept for compatibility;
 * the gpu shader reads these as floats after the upload step casts them. */
typedef struct {
    double re;
    double im;
} ComplexFloat;

// series approximation coefficients for skipping iterations
typedef struct {
    double A_re, A_im;
    double B_re, B_im;
    double C_re, C_im;
} SACoeff;

// represents the stored reference orbit path for the center coordinate
typedef struct {
    ComplexFloat* zn;  // array of orbit points z_0, z_1, ..., z_n
    SACoeff* sa;       // taylor series approximation coefficients
    int len;           // length of the orbit path before escaping
} RefOrbit;

/* computes the reference orbit starting at z0 = 0.0 for the given center coordinates.
 * the orbit calculations are performed at high precision and stored in float format.
 * stops and truncates early if the point escapes the escape radius. */
RefOrbit* perturbation_compute(precise_float center_re, precise_float center_im, int max_iter);

// safely frees the reference orbit array and container
void perturbation_free(RefOrbit* orbit);

/* same as perturbation_compute() but uses BigNum arithmetic for the reference orbit.
 * for use when zoom < BIGNUM_ZOOM_THRESHOLD, where double precision has lost all bits.
 * the output RefOrbit is identical — orbit values are downcast to double for gpu upload. */
RefOrbit* perturbation_compute_bignum(double center_re, double center_im, int max_iter);

/* result from find_best_ref_point() */
typedef struct {
    precise_float ref_re;
    precise_float ref_im;
    float offset_x;  // normalized screen offset of chosen point [-0.5, 0.5]
    float offset_y;
} RefPoint;

/*
 * scans a grid of candidate points within the current view and picks the one
 * with the highest escape iteration count. favoring deep points avoids placing
 * the reference orbit on a pixel that escapes early, which would produce
 * blocky artifacts across the whole frame.
 */
RefPoint find_best_ref_point(precise_float center_re, precise_float center_im, precise_float zoom,
                             precise_float aspect, int max_iters, int grid_size);

#endif  // PERTURBATION_H
