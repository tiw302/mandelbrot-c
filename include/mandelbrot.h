#ifndef MANDELBROT_H
#define MANDELBROT_H

#include "config.h"

typedef struct {
    double re;
    double im;
} complex_t;

/**
 * Returns fractional iteration count for point c in the Mandelbrot set.
 * Returns MAX_ITERATIONS if the point is within the set.
 */
double mandelbrot_check(complex_t c);

#endif
