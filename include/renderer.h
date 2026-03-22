#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include "config.h"
#include "mandelbrot.h"

/* Selects which fractal the render threads should compute. */
typedef enum {
    RENDER_MANDELBROT = 0,
    RENDER_JULIA      = 1
} RenderMode;

/*
 * Work packet passed to each render thread.
 * Both Mandelbrot and Julia modes share this struct; julia_c is only
 * meaningful when mode == RENDER_JULIA.
 */
typedef struct {
    int        id;
    Uint32    *pixels;
    int        pitch;
    int        start_y;
    int        end_y;
    int        window_width;
    int        window_height;
    double     re_min, re_max;
    double     im_min, im_max;
    RenderMode mode;     /* which fractal to render */
    complex_t  julia_c;  /* fixed c parameter for Julia mode */
} thread_data_t;

/**
 * @brief Map an iteration count to an RGB colour using sine-wave gradients.
 *
 * Points inside the set (iterations == MAX_ITERATIONS) are rendered black.
 */
void get_color(int iterations, Uint8 *r, Uint8 *g, Uint8 *b);

/** @brief Thread entry point -- renders the horizontal band defined in arg. */
void *render_thread(void *arg);

/**
 * @brief Render the Mandelbrot set into `pixels` using THREAD_COUNT threads.
 */
void render_mandelbrot_threaded(Uint32 *pixels, int pitch,
                                int window_width, int window_height,
                                double re_min, double re_max,
                                double im_min, double im_max);

/**
 * @brief Render the Julia set J_c into `pixels` using THREAD_COUNT threads.
 *
 * @param julia_c  The fixed complex parameter that defines which Julia set to draw.
 *                 Typically the complex-plane coordinate under the mouse cursor.
 */
void render_julia_threaded(Uint32 *pixels, int pitch,
                           int window_width, int window_height,
                           double re_min, double re_max,
                           double im_min, double im_max,
                           complex_t julia_c);

#endif /* RENDERER_H */
