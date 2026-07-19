/* core_math.h — umbrella header for all fractal math kernels.
 *
 * provides the shared complex_t type, then pulls in mandelbrot and julia
 * headers so callers only need a single #include. the 128-bit simd_f128
 * type is conditionally included when building the high-precision engine.
 */

#ifndef CORE_MATH_H
#define CORE_MATH_H

#include "config.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include "math_types.h"

/* 128-bit double-double type for extreme zoom precision. * only available when building with
 * -DUSE_SIMD_F128. */
#ifdef USE_SIMD_F128
#define SIMD_F128_IMPLEMENTATION
#include "../third_party/simd-f128/simd_f128.h"
#include "../third_party/simd-f128/simd_f128_complex.h"
#include "../third_party/simd-f128/simd_f128_math.h"
#include "../third_party/simd-f128/simd_f128_utils.h"
#ifdef __AVX2__
#include "../third_party/simd-f128/simd_f128_vector.h"
#endif
#endif

// pull in individual kernel headers — they depend on complex_t above
#include "buffalo.h"
#include "burning_ship.h"
#include "celtic.h"
#include "fractal.h"
#include "julia.h"
#include "mandelbrot.h"
#include "tricorn.h"

#endif // CORE_MATH_H
