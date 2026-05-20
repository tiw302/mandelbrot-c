/* renderer.h — multi-threaded cpu rendering pipeline.
 *
 * provides a persistent thread pool that parks between frames to avoid
 * pthread_create/join overhead. workers claim scanlines via an atomic
 * counter for lock-free load balancing across cpu cores. */

#ifndef RENDER_H
#define RENDER_H

#include <stdatomic.h>
#include <stdint.h>

#include "color.h"
#include "config.h"
#include "mandelbrot.h"

typedef enum { RENDER_MANDELBROT = 0, RENDER_JULIA = 1 } RenderMode;

/* thread pool lifecycle */
void init_renderer(int max_iterations, int palette_idx);
void cleanup_renderer(void);

/* query thread pool state */
int get_optimal_thread_count(void);
int get_actual_thread_count(void);

/* high-level render dispatch — blocks until all rows are painted */
void render_mandelbrot_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                                double re_min, double re_max, double im_min, double im_max,
                                int max_iterations);

void render_julia_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                           double re_min, double re_max, double im_min, double im_max,
                           complex_t julia_c, int max_iterations);

/* legacy symbol — kept for backward compatibility */
void* render_thread(void* arg);

#endif