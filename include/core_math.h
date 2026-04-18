#ifndef CORE_MATH_H
#define CORE_MATH_H

#include "config.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

typedef struct {
    double re;
    double im;
} complex_t;

// Mandelbrot functions
// get iteration count for mandelbrot point
// returns max_iterations if it's in the set
double mandelbrot_check(complex_t c, int max_iterations);

#ifdef __AVX2__
// simd version: process 4 pixels at once
void mandelbrot_check_avx2(const double *re, const double *im, int max_iterations, double *results);
#endif

#ifdef __wasm_simd128__
// wasm simd path
void mandelbrot_check_wasm_simd128(const double *re, const double *im, int max_iterations, double *results);
#endif

// Julia functions
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
