#ifndef JULIA_H
#define JULIA_H

#include "config.h"
#include "mandelbrot.h"

/**
 * Returns fractional iteration count for point z in the Julia set defined by c.
 * Returns MAX_ITERATIONS if the point is within the set.
 */
double julia_check(complex_t z, complex_t c);

#endif
