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

double mandelbrot_check(complex_t c, int max_iterations);

#ifdef __AVX2__
void mandelbrot_check_avx2(const double *re, const double *im, int max_iterations, double *results);
#endif

#ifdef __wasm_simd128__
void mandelbrot_check_wasm_simd128(const double *re, const double *im, int max_iterations, double *results);
#endif

double julia_check(complex_t z, complex_t c, int max_iterations);

#ifdef __AVX2__
void julia_check_avx2(const double *re, const double *im, complex_t c, int max_iterations, double *results);
#endif

#ifdef __wasm_simd128__
void julia_check_wasm_simd128(const double *re, const double *im, complex_t c, int max_iterations, double *results);
#endif

#endif