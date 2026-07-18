/* math_types.h
 *
 * arbitrary precision math types and structs.
 */

#ifndef MATH_TYPES_H
#define MATH_TYPES_H

// shared complex number type
typedef struct {
    double re;
    double im;
} complex_t;

// available fractal render modes
typedef enum {
    RENDER_MANDELBROT = 0,
    RENDER_JULIA = 1,
    RENDER_BURNING_SHIP = 2,
    RENDER_TRICORN = 3,
    RENDER_CELTIC = 4,
    RENDER_BUFFALO = 5
} RenderMode;

/* high-precision floating point type for deep zooms.
 * note: MSVC does not support __float128. on Windows MSVC, this falls back
 * to 'long double' (which is identical to 64-bit 'double' in MSVC).
 * this limits native MSVC builds to ~1e-15 zoom. */
#ifdef __SIZEOF_FLOAT128__
typedef __float128 precise_float;
#else
typedef long double precise_float;
#endif

// camera state in the complex plane
typedef struct {
    precise_float center_re;
    precise_float center_im;
    precise_float zoom;
} ViewState;

#endif  // math_types_h
