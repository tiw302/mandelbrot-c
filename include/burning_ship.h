/* burning_ship.h — public api for the burning ship fractal.
 *
 * the burning ship fractal is a variant of the mandelbrot set where
 * the real and imaginary parts of z are replaced with their absolute
 * values before squaring: z = (|re(z)| + i*|im(z)|)^2 + c.
 * this produces an asymmetric, ship-shaped fractal. */

#ifndef BURNING_SHIP_H
#define BURNING_SHIP_H

/* note: burning_ship.h ↔ core_math.h have a circular include, but the
 * include guards make this safe — complex_t is always defined before
 * this header's declarations are processed. */
#include "config.h"
#include "core_math.h"

// scalar path: processes one pixel at a time
double burning_ship_check(complex_t c, int max_iterations);

// avx2 vectorized path: processes 4 pixels simultaneously
#ifdef __AVX2__
#include <immintrin.h>
void burning_ship_check_avx2(__m256d cre, __m256d cim, int max_iterations, double* results);
#endif

// avx-512 vectorized path: processes 8 pixels simultaneously
#ifdef __AVX512F__
#include <immintrin.h>
void burning_ship_check_avx512(__m512d cre, __m512d cim, int max_iterations, double* results);
#endif

// wasm simd128 path: processes 2 pixels simultaneously
#ifdef __wasm_simd128__
#include <wasm_simd128.h>
void burning_ship_check_wasm_simd128(v128_t cre, v128_t cim, int max_iterations, double* results);
#endif

// high-precision 128-bit path (double-double): single pixel
#ifdef USE_SIMD_F128
double burning_ship_check_f128(simd_f128 cre, simd_f128 cim, int max_iterations);
#ifdef __AVX2__
// high-precision 128-bit AVX2 path: processes 4 pixels simultaneously
void burning_ship_check_f128x4(simd_f128x4 cre, simd_f128x4 cim, int max_iterations,
                               double* results);
#endif
#endif

#endif
#ifdef __ARM_NEON
#include <arm_neon.h>
void burning_ship_check_neon(float64x2_t cre, float64x2_t cim, int max_iterations, double* results);
#endif
