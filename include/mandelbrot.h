#ifndef MANDELBROT_H
#define MANDELBROT_H

#include "config.h"
#include "core_math.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __AVX2__
void mandelbrot_check_avx2(const double *re, const double *im, int max_iterations, double *results);
#endif

#ifdef __wasm_simd128__
void mandelbrot_check_wasm_simd128(const double *re, const double *im, int max_iterations, double *results);
#endif

#endif