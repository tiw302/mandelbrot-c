#ifndef JULIA_H
#define JULIA_H

#include "mandelbrot.h"

// get iteration count for julia set point
// returns max_iterations if it's in the set
double julia_check(complex_t z, complex_t c, int max_iterations);

#ifdef __AVX2__
// simd version: process 4 pixels at once
void julia_check_avx2(const double *re, const double *im, complex_t c, int max_iterations, double *results);
#endif

#ifdef __wasm_simd128__
// wasm simd path
void julia_check_wasm_simd128(const double *re, const double *im, complex_t c, int max_iterations, double *results);
#endif

#endif
