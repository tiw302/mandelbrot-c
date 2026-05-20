/* julia.h — public api for julia set iteration kernels.
 *
 * mirrors the mandelbrot api but takes an additional constant c parameter.
 * unlike mandelbrot, julia sets use the pixel coordinate as initial z
 * and keep c fixed across the entire fractal. */

#ifndef JULIA_H
#define JULIA_H

#include "mandelbrot.h"

/* scalar path */
double julia_check(complex_t z, complex_t c, int max_iterations);

/* avx2 vectorized path — 4 pixels at once */
#ifdef __AVX2__
void julia_check_avx2(const double* re, const double* im, complex_t c, int max_iterations,
                      double* results);
#endif

/* wasm simd128 path — 2 pixels at once */
#ifdef __wasm_simd128__
void julia_check_wasm_simd128(const double* re, const double* im, complex_t c, int max_iterations,
                              double* results);
#endif

/* 128-bit double-double path */
#ifdef USE_SIMD_F128
double julia_check_f128(simd_f128 zre, simd_f128 zim, simd_f128 cre, simd_f128 cim, int max_iterations);
#endif

#endif