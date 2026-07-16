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
    ComplexFloat* zn; // array of orbit points z_0, z_1, ..., z_n
    SACoeff* sa;      // taylor series approximation coefficients
    int len;          // length of the orbit path before escaping
} RefOrbit;

/* computes the reference orbit starting at z0 = 0.0 for the given center coordinates.
 * the orbit calculations are performed at high precision and stored in float format.
 * stops and truncates early if the point escapes the escape radius. */
RefOrbit* perturbation_compute(precise_float center_re, precise_float center_im, int max_iter);

// safely frees the reference orbit array and container
void perturbation_free(RefOrbit* orbit);

#endif // perturbation_h
