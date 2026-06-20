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
#include "core_math.h"

typedef enum { RENDER_MANDELBROT = 0, RENDER_JULIA = 1, RENDER_BURNING_SHIP = 2 } RenderMode;

// thread pool lifecycle
void init_renderer(int max_iterations, int palette_idx);
void cleanup_renderer(void);

// query thread pool state
int get_optimal_thread_count(void);
int get_actual_thread_count(void);

// high-level render dispatch — blocks until all rows are painted
void render_mandelbrot_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                                double re_min, double re_max, double im_top, double im_bottom,
                                int max_iterations);

void render_julia_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                           double re_min, double re_max, double im_top, double im_bottom,
                           complex_t julia_c, int max_iterations);

void render_burning_ship_threaded(uint32_t* pixels, int pitch, int window_width, int window_height,
                                  double re_min, double re_max, double im_top, double im_bottom,
                                  int max_iterations);

// legacy symbol — kept for backward compatibility
void* render_thread(void* arg);

// dynamic precision control
void set_cpu_precision(int use_128bit);
int get_cpu_precision(void);

#endif