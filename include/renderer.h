#ifndef RENDERER_H
#define RENDERER_H

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
    atomic_int      *next_row;
} thread_data_t;

/* Initialize global rendering resources (LUT) */
void init_renderer(void);

/* Maps fractional iteration to RGB using sine-wave gradients */
void get_color(double iterations, Uint8 *r, Uint8 *g, Uint8 *b);

/* Thread entry point for row-based work queue */
void *render_thread(void *arg);

/* Threaded Mandelbrot rendering entry point */
void render_mandelbrot_threaded(Uint32 *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max);

/* Threaded Julia rendering entry point */
void render_julia_threaded(Uint32 *pixels, int pitch,
                           int window_width, int window_height,
                           double re_min, double re_max,
                           double im_min, double im_max,
                           complex_t julia_c);

#endif
