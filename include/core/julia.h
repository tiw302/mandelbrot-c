/* julia.h — julia set escape-time kernels.
 *
 * similar to mandelbrot but uses a fixed c-parameter across the whole frame.
 * the initial z coordinate is derived from the pixel position.
 */

#ifndef JULIA_H
#define JULIA_H

#include "mandelbrot.h"

// scalar path: single pixel
double julia_check(complex_t z, complex_t c, int max_iterations);

// avx2 path: 4 pixels simultaneously
#ifdef __AVX2__
#include <immintrin.h>
void julia_check_avx2(__m256d zre, __m256d zim, complex_t c, int max_iterations, double* results);
#endif

// avx-512 path: 8 pixels simultaneously
#ifdef __AVX512F__
#include <immintrin.h>
void julia_check_avx512(__m512d zre, __m512d zim, complex_t c, int max_iterations, double* results);
#endif

// wasm simd128 path: 2 pixels simultaneously
#ifdef __wasm_simd128__
#include <wasm_simd128.h>
void julia_check_wasm_simd128(v128_t zre, v128_t zim, complex_t c, int max_iterations,
                              double* results);
#endif

// high-precision 128-bit paths
#ifdef USE_SIMD_F128
double julia_check_f128(simd_f128 zre, simd_f128 zim, simd_f128 cre, simd_f128 cim,
                        int max_iterations);
#ifdef __AVX2__
void julia_check_f128x4(simd_f128x4 zre, simd_f128x4 zim, simd_f128x4 cre, simd_f128x4 cim,
                        int max_iterations, double* results);
#endif
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
void julia_check_neon(float64x2_t zre, float64x2_t zim, float64x2_t cre, float64x2_t cim,
                      int max_iterations, double* results);
#endif

#endif  // JULIA_H
