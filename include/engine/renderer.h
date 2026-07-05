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

// opaque declaration of RendererContext
typedef struct RendererContext RendererContext;

// thread pool lifecycle
RendererContext* init_renderer(int max_iterations, int palette_idx);
void cleanup_renderer(RendererContext* ctx);

// query thread pool state
int get_optimal_thread_count(void);
int get_actual_thread_count(const RendererContext* ctx);
int set_renderer_thread_count(RendererContext* ctx, int count);

// encapsulating all render job parameters
typedef struct {
    uint32_t* pixels;
    int pitch;
    int window_width;
    int window_height;
    precise_float re_min;
    precise_float re_max;
    precise_float im_top;
    precise_float im_bottom;
    RenderMode mode;
    complex_t julia_c;
    int max_iterations;
} RenderJob;

// high-level render dispatch — blocks until all rows are painted
void render_fractal_threaded(RendererContext* ctx, const RenderJob* job);


// dynamic precision control
void set_cpu_precision(RendererContext* ctx, int use_128bit);
int get_cpu_precision(const RendererContext* ctx);

#endif