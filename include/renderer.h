#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include <stdatomic.h>
#include "config.h"
#include "mandelbrot.h"

typedef enum {
    RENDER_MANDELBROT = 0,
    RENDER_JULIA      = 1
} RenderMode;

typedef struct {
    int              id;
    Uint32          *pixels;
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

#define PALETTE_COUNT 4

// precompute LUT or other startup work
void init_renderer(int max_iterations, int palette_idx);

// get the number of threads actually being used
int get_actual_thread_count(void);

// map iteration to RGB color
void get_color(double iterations, int max_iterations, Uint8 *r, Uint8 *g, Uint8 *b);

// main worker thread function
void *render_thread(void *arg);

// multi-threaded mandelbrot renderer
void render_mandelbrot_threaded(Uint32 *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max,
                                int max_iterations);

// multi-threaded julia renderer
void render_julia_threaded(Uint32 *pixels, int pitch,
                           int window_width, int window_height,
                           double re_min, double re_max,
                           double im_min, double im_max,
                           complex_t julia_c,
                           int max_iterations);

#endif
