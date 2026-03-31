#ifndef JULIA_H
#define JULIA_H

#include "config.h"
#include "mandelbrot.h"

// get iteration count for julia set point
// returns MAX_ITERATIONS if it's in the set
double julia_check(complex_t z, complex_t c);

#ifdef __AVX2__
// SIMD version: process 4 pixels at once
void julia_check_avx2(const double *re, const double *im, complex_t c, double *results);
#endif

#endif
