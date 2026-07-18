/* tricorn.h
 *
 * tricorn fractal mathematical definitions.
 */

#ifndef TRICORN_H
#define TRICORN_H

#include "core_math.h"

// standard 64-bit precision kernels
double tricorn_check(complex_t c, int max_iterations);
#ifdef __AVX2__
void tricorn_check_avx2(__m256d cre, __m256d cim, int max_iterations, double* results);
#endif
#ifdef __AVX512F__
void tricorn_check_avx512(__m512d cre, __m512d cim, int max_iterations, double* results);
#endif
#ifdef __wasm_simd128__
void tricorn_check_wasm_simd128(v128_t cre, v128_t cim, int max_iterations, double* results);
#endif
#ifdef __ARM_NEON
void tricorn_check_neon(float64x2_t cre, float64x2_t cim, int max_iterations, double* results);
#endif

// high-precision 128-bit kernels
#ifdef USE_SIMD_F128
double tricorn_check_f128(simd_f128 cre, simd_f128 cim, int max_iterations);
#ifdef __AVX2__
void tricorn_check_f128x4(simd_f128x4 cre, simd_f128x4 cim, int max_iterations, double* results);
#endif
#endif

#endif
