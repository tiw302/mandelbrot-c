#ifndef JULIA_H
#define JULIA_H

#include "config.h"
#include "mandelbrot.h"

// get iteration count for julia set point
// returns MAX_ITERATIONS if it's in the set
double julia_check(complex_t z, complex_t c);

#endif
