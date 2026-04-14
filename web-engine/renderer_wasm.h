#ifndef RENDERER_WASM_H
#define RENDERER_WASM_H

#include <stdint.h>
#include "../include/config.h"
#include "../include/mandelbrot.h"
#include "../include/julia.h"

void render_mandelbrot_wasm(uint32_t *pixels, int pitch,
                            int window_width, int window_height,
                            double re_min, double re_max,
                            double im_min, double im_max,
                            int max_iterations);

void render_julia_wasm(uint32_t *pixels, int pitch,
                       int window_width, int window_height,
                       double re_min, double re_max,
                       double im_min, double im_max,
                       complex_t julia_c,
                       int max_iterations);

#endif
