/* core_math.h — umbrella header for all fractal math kernels.
 *
 * provides the shared complex_t type, then pulls in mandelbrot and julia
 * headers so callers only need a single #include. the 128-bit simd_f128
 * type is conditionally included when building the high-precision engine. */

#ifndef CORE_MATH_H
#define CORE_MATH_H

#include "config.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

/* shared complex number type used by all math kernels */
typedef struct {
    double re;
    double im;
} complex_t;

/* 128-bit double-double type for extreme zoom precision.
 * only available when building with -DUSE_SIMD_F128. */
#ifdef USE_SIMD_F128
#define SIMD_F128_IMPLEMENTATION
#include "../third_party/simd-f128/simd_f128.h"
#endif

/* pull in individual kernel headers — they depend on complex_t above */
#include "mandelbrot.h"
#include "julia.h"
#include "burning_ship.h"

#endif