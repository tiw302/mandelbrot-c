#ifndef MANDELBROT_H
#define MANDELBROT_H

#include "config.h"

typedef struct {
    double re;
    double im;
} complex_t;

// get iteration count for mandelbrot point
// returns MAX_ITERATIONS if it's in the set
double mandelbrot_check(complex_t c);

#endif
