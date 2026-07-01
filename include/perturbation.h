/* perturbation.h
 *
 * interface for calculating and managing the high-precision reference orbit
 * on the cpu. this reference orbit is uploaded to the gpu as a texture for
 * delta calculations in perturbation rendering mode.
 */

#ifndef PERTURBATION_H
#define PERTURBATION_H

#include "config.h"

// single-precision complex number format for gpu texture compatibility
typedef struct {
    double re;
    double im;
} ComplexFloat;

// represents the stored reference orbit path for the center coordinate
typedef struct {
    ComplexFloat* zn; // array of orbit points z_0, z_1, ..., z_n
    int len;          // length of the orbit path before escaping
} RefOrbit;

/* computes the reference orbit starting at z0 = 0.0 for the given center coordinates.
 * the orbit calculations are performed at high precision and stored in float format.
 * stops and truncates early if the point escapes the escape radius. */
RefOrbit* perturbation_compute(precise_float center_re, precise_float center_im, int max_iter);

// safely frees the reference orbit array and container
void perturbation_free(RefOrbit* orbit);

#endif // perturbation.h
