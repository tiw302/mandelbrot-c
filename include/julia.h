#ifndef JULIA_H
#define JULIA_H

#include "mandelbrot.h"

double julia_check(complex_t z, complex_t c, int max_iterations);

#ifdef __AVX2__
void julia_check_avx2(const double *re, const double *im, complex_t c, int max_iterations, double *results);
#endif

#ifdef __wasm_simd128__
void julia_check_wasm_simd128(const double *re, const double *im, complex_t c, int max_iterations, double *results);
#endif

#endif