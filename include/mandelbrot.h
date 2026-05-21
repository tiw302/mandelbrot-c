/* mandelbrot.h — public api for mandelbrot set iteration kernels.
 *
 * provides scalar, simd-vectorized, and high-precision 128-bit variants
 * of the mandelbrot escape-time algorithm. all functions return a smooth
 * (fractional) iteration count for continuous coloring. */

#ifndef MANDELBROT_H
#define MANDELBROT_H

/* note: mandelbrot.h ↔ core_math.h have a circular include, but the
 * include guards make this safe — core_math.h defines complex_t first,
 * then pulls in this header. direct includers can use either entry point. */
#include "config.h"
#include "core_math.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/* scalar path — processes one pixel at a time */
double mandelbrot_check(complex_t c, int max_iterations);

/* avx2 vectorized path — processes 4 pixels simultaneously (x86_64 desktop) */
#ifdef __AVX2__
void mandelbrot_check_avx2(const double* re, const double* im, int max_iterations, double* results);
#endif

/* wasm simd128 path — processes 2 pixels simultaneously (browser) */
#ifdef __wasm_simd128__
void mandelbrot_check_wasm_simd128(const double* re, const double* im, int max_iterations,
                                   double* results);
#endif

/* 128-bit double-double path — single pixel, extreme precision.
 * simd_f128 type is provided by core_math.h when USE_SIMD_F128 is defined. */
#ifdef USE_SIMD_F128
double mandelbrot_check_f128(simd_f128 cre, simd_f128 cim, int max_iterations);
#endif

#endif