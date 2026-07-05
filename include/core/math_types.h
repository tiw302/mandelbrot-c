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

// high-precision floating point type for deep zooms
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

#endif // math_types_h
