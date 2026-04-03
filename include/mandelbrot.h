#ifndef MANDELBROT_H
#define MANDELBROT_H

#include "config.h"

#ifdef __AVX2__
#include <immintrin.h>
#endif

typedef struct {
    double re;
    double im;
} complex_t;

// get iteration count for mandelbrot point
// returns max_iterations if it's in the set
double mandelbrot_check(complex_t c, int max_iterations);

#ifdef __AVX2__
// SIMD version: process 4 pixels at once
void mandelbrot_check_avx2(const double *re, const double *im, int max_iterations, double *results);
#endif

#endif
