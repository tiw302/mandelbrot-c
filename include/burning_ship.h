/* burning_ship.h — public api for the burning ship fractal.
 *
 * the burning ship fractal is a variant of the mandelbrot set where
 * the real and imaginary parts of z are replaced with their absolute
 * values before squaring: z = (|re(z)| + i*|im(z)|)^2 + c.
 * this produces an asymmetric, ship-shaped fractal. */

#ifndef BURNING_SHIP_H
#define BURNING_SHIP_H

#include "config.h"

#include "core_math.h"
/* scalar path */
double burning_ship_check(complex_t c, int max_iterations);

/* avx2 vectorized path — 4 pixels at once */
#ifdef __AVX2__
void burning_ship_check_avx2(const double* re, const double* im, int max_iterations,
                              double* results);
#endif

/* wasm simd128 path — 2 pixels at once */
#ifdef __wasm_simd128__
void burning_ship_check_wasm_simd128(const double* re, const double* im, int max_iterations,
                                      double* results);
#endif

#endif