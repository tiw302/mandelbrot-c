#ifndef RENDER_H
#define RENDER_H

#include <stdatomic.h>
#include <stdint.h>
#include "config.h"
#include "mandelbrot.h"
#include "color.h"

// optimal thread count detection
int get_optimal_thread_count(void);

typedef enum {
    RENDER_MANDELBROT = 0,
    RENDER_JULIA      = 1
} RenderMode;

typedef struct {
    int              id;
    uint32_t        *pixels;
    int              pitch;
    int              window_width;
    int              window_height;
    double           re_min, re_max;
    double           im_min, im_max;
    RenderMode       mode;
    complex_t        julia_c;
    int              max_iterations;
    atomic_int      *next_row;
} thread_data_t;

// renderer lifecycle
void init_renderer(int max_iterations, int palette_idx);
void cleanup_renderer(void);

// state access
int get_actual_thread_count(void);

// core worker
void *render_thread(void *arg);

// high-level rendering api
void render_mandelbrot_threaded(uint32_t *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max,
                                int max_iterations);

void render_julia_threaded(uint32_t *pixels, int pitch,
                           int window_width, int window_height,
                           double re_min, double re_max,
                           double im_min, double im_max,
                           complex_t julia_c,
                           int max_iterations);

#endif
