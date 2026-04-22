#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(__EMSCRIPTEN__)
    #ifndef SOKOL_GLES3
    #define SOKOL_GLES3
    #endif
#else
    #define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL

#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"

#include "config.h"
#include "core_math.h"
#include "renderer.h"
#include "tour.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

static void slog_func(const char* tag, uint32_t log_level, uint32_t log_item_id, const char* message_or_null, uint32_t line_nr, const char* filename_or_null, void* user_data) {
    (void)tag; (void)log_level; (void)log_item_id;
    if (message_or_null) {
        printf("[sokol][%d] %s (line: %u, file: %s)\n", log_level, message_or_null, line_nr, filename_or_null ? filename_or_null : "unknown");
    }
}

typedef struct {
    float center[2];
    float zoom;
    float iters;
    float aspect;
    float julia_c[2];
    float is_julia;
} params_t;

typedef struct {
    sg_pipeline pip_cpu, pip_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t *pixels;
    int win_w, win_h;

    ViewState view;
    ViewState history[MAX_HISTORY_SIZE];
    int history_count;

    TourState m_tour;
    int julia_mode, gpu_mode;
    complex_t julia_c;
    int max_iterations, palette_idx;
    int needs_redraw;
    int is_panning, is_zooming;
    int last_mouse_x, last_mouse_y;
    int mouse_x, mouse_y;
} GlobalCtx;

static GlobalCtx ctx;

#include "shaders.h"

static void init(void) {
    sg_setup(&(sg_desc){ 
        .environment = sglue_environment(), 
        .logger.func = slog_func 
    });
    stm_setup();

    ctx.win_w = sapp_width(); 
    ctx.win_h = sapp_height();
    if (ctx.win_w <= 0) ctx.win_w = 1024;
    if (ctx.win_h <= 0) ctx.win_h = 768;
    
    ctx.pixels = (uint32_t*) malloc((size_t)ctx.win_w * ctx.win_h * 4);
    if (!ctx.pixels) {
        printf("[sokol] fatal: failed to allocate pixel buffer\n");
    }

    float verts[] = { 
        -1.0f,  1.0f, 0.0f, 0.0f, 
         1.0f,  1.0f, 1.0f, 0.0f, 
         1.0f, -1.0f, 1.0f, 1.0f, 
        -1.0f, -1.0f, 0.0f, 1.0f 
    };
    ctx.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){ .data = SG_RANGE(verts) });
    
    uint16_t idx[] = { 0, 1, 2, 0, 2, 3 };
    ctx.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){ 
        .usage = { .index_buffer = true }, 
        .data = SG_RANGE(idx) 
    });

    ctx.img = sg_make_image(&(sg_image_desc){ 
        .width = ctx.win_w, 
        .height = ctx.win_h, 
        .pixel_format = SG_PIXELFORMAT_RGBA8, 
        .usage = { .dynamic_update = true } 
    });
    ctx.img_view = sg_make_view(&(sg_view_desc){ .texture.image = ctx.img });
    ctx.smp = sg_make_sampler(&(sg_sampler_desc){ 
        .min_filter = SG_FILTER_LINEAR, 
        .mag_filter = SG_FILTER_LINEAR 
    });
    
    ctx.bind.views[0] = ctx.img_view; 
    ctx.bind.samplers[0] = ctx.smp;

    sg_shader shd_cpu = sg_make_shader(&(sg_shader_desc){
        .attrs[0].glsl_name = "pos",
        .attrs[1].glsl_name = "uv_in",
        .vertex_func.source = vs_src, 
        .fragment_func.source = fs_cpu_src,
        .texture_sampler_pairs[0] = { 
            .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0, .glsl_name = "tex" 
        }
    });
    
    ctx.pip_cpu = sg_make_pipeline(&(sg_pipeline_desc){ 
        .shader = shd_cpu, 
        .layout = {
            .attrs[0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            .attrs[1] = { .format = SG_VERTEXFORMAT_FLOAT2 }
        },
        .index_type = SG_INDEXTYPE_UINT16 
    });
    
    sg_shader shd_gpu = sg_make_shader(&(sg_shader_desc){
        .attrs[0].glsl_name = "pos",
        .attrs[1].glsl_name = "uv_in",
        .vertex_func.source = vs_src, 
        .fragment_func.source = fs_gpu_src,
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT, 
            .size = sizeof(params_t),
            .glsl_uniforms = { 
                { .glsl_name = "u_center", .type = SG_UNIFORMTYPE_FLOAT2 }, 
                { .glsl_name = "u_zoom", .type = SG_UNIFORMTYPE_FLOAT }, 
                { .glsl_name = "u_iters", .type = SG_UNIFORMTYPE_FLOAT }, 
                { .glsl_name = "u_aspect", .type = SG_UNIFORMTYPE_FLOAT },
                { .glsl_name = "u_julia_c", .type = SG_UNIFORMTYPE_FLOAT2 },
                { .glsl_name = "u_is_julia", .type = SG_UNIFORMTYPE_FLOAT }
            }
        }
    });
    
    ctx.pip_gpu = sg_make_pipeline(&(sg_pipeline_desc){ 
        .shader = shd_gpu, 
        .layout = {
            .attrs[0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            .attrs[1] = { .format = SG_VERTEXFORMAT_FLOAT2 }
        },
        .index_type = SG_INDEXTYPE_UINT16 
    });

    ctx.pass_action = (sg_pass_action){ 
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = {0,0,0,1} } 
    };
    
    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.julia_c = (complex_t){ -0.8, 0.156 }; 
    ctx.max_iterations = DEFAULT_ITERATIONS; 
    ctx.needs_redraw = 1; 
    
#if defined(FORCE_GPU_MODE)
    ctx.gpu_mode = 1;
#else
    ctx.gpu_mode = 0;
#endif

    init_renderer(ctx.max_iterations, 0);
}

static void frame(void) {
    if (ctx.m_tour.phase != TOUR_IDLE) { 
        update_tour(&ctx.m_tour, &ctx.view, (uint32_t)stm_ms(stm_now())); 
        ctx.needs_redraw = 1; 
    }

    if (!ctx.gpu_mode && ctx.needs_redraw) {
        double rmin, rmax, imin, imax;
        double aspect = (double)ctx.win_w/ctx.win_h;
        imin = ctx.view.center_im - ctx.view.zoom/2; 
        imax = ctx.view.center_im + ctx.view.zoom/2;
        rmin = ctx.view.center_re - (ctx.view.zoom * aspect)/2; 
        rmax = ctx.view.center_re + (ctx.view.zoom * aspect)/2;
        
        if (ctx.julia_mode) {
            render_julia_threaded(ctx.pixels, ctx.win_w*4, ctx.win_w, ctx.win_h, rmin, rmax, imin, imax, ctx.julia_c, ctx.max_iterations);
        } else {
            render_mandelbrot_threaded(ctx.pixels, ctx.win_w*4, ctx.win_w, ctx.win_h, rmin, rmax, imin, imax, ctx.max_iterations);
        }
        
        sg_update_image(ctx.img, &(sg_image_data){ 
            .mip_levels[0] = { .ptr = ctx.pixels, .size = (size_t)ctx.win_w*ctx.win_h*4 } 
        });
        ctx.needs_redraw = 0;
    }

    sg_begin_pass(&(sg_pass){ .action = ctx.pass_action, .swapchain = sglue_swapchain() });
    
    sg_pipeline cur_pip = ctx.gpu_mode ? ctx.pip_gpu : ctx.pip_cpu;
    if (sg_query_pipeline_state(cur_pip) == SG_RESOURCESTATE_VALID) {
        sg_apply_pipeline(cur_pip);
        sg_apply_bindings(&ctx.bind);
        if (ctx.gpu_mode) {
            params_t params = {
                .center = { (float)ctx.view.center_re, (float)ctx.view.center_im },
                .zoom = (float)ctx.view.zoom,
                .iters = (float)ctx.max_iterations,
                .aspect = (float)ctx.win_w / ctx.win_h,
                .julia_c = { (float)ctx.julia_c.re, (float)ctx.julia_c.im },
                .is_julia = ctx.julia_mode ? 1.0f : 0.0f
            };
            sg_apply_uniforms(0, &SG_RANGE(params));
        }
        sg_draw(0, 6, 1);
    }

    sg_end_pass(); 
    sg_commit();
}

static void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) { 
            ctx.is_panning = 1; 
            ctx.last_mouse_x = (int)ev->mouse_x; 
            ctx.last_mouse_y = (int)ev->mouse_y; 
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP) {
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ctx.is_panning = 0;
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        if (ctx.is_panning) {
            double aspect = (double)ctx.win_w/ctx.win_h;
            ctx.view.center_re -= (ev->mouse_x - ctx.last_mouse_x) * (ctx.view.zoom * aspect) / ctx.win_w;
            ctx.view.center_im += (ev->mouse_y - ctx.last_mouse_y) * ctx.view.zoom / ctx.win_h;
            ctx.last_mouse_x = (int)ev->mouse_x; 
            ctx.last_mouse_y = (int)ev->mouse_y; 
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        ctx.view.zoom *= (ev->scroll_y > 0) ? 0.9 : 1.1; 
        ctx.needs_redraw = 1;
    } else if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (ev->key_code == SAPP_KEYCODE_G) { 
            ctx.gpu_mode = !ctx.gpu_mode; 
            ctx.needs_redraw = 1; 
        }
        else if (ev->key_code == SAPP_KEYCODE_R) { 
            ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM}; 
            ctx.needs_redraw = 1; 
        }
    } else if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        ctx.win_w = (int)ev->framebuffer_width;
        ctx.win_h = (int)ev->framebuffer_height;
        if (ctx.pixels) free(ctx.pixels);
        ctx.pixels = (uint32_t*) malloc((size_t)ctx.win_w * ctx.win_h * 4);
    if (!ctx.pixels) {
        printf("[sokol] fatal: failed to allocate pixel buffer\n");
        return;
    }
        
        sg_destroy_image(ctx.img);
        ctx.img = sg_make_image(&(sg_image_desc){ 
            .width = ctx.win_w, 
            .height = ctx.win_h, 
            .pixel_format = SG_PIXELFORMAT_RGBA8, 
            .usage = { .dynamic_update = true } 
        });
        sg_destroy_view(ctx.img_view);
        ctx.img_view = sg_make_view(&(sg_view_desc){ .texture.image = ctx.img });
        ctx.bind.views[0] = ctx.img_view;
        ctx.needs_redraw = 1;
    }
}

static void cleanup(void) { 
    free(ctx.pixels); 
    cleanup_renderer(); 
    sg_shutdown(); 
}

#if defined(__EMSCRIPTEN__)

EMSCRIPTEN_KEEPALIVE
void wasm_reset_view(void) {
    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_undo_zoom(void) {
    if (ctx.history_count > 0) {
        ctx.view = ctx.history[--ctx.history_count];
        ctx.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_tour(void) {
    if (ctx.m_tour.phase == TOUR_IDLE) {
        start_tour(&ctx.m_tour, &ctx.view);
    } else {
        stop_tour(&ctx.m_tour);
    }
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_next_palette(void) {
    ctx.palette_idx = (ctx.palette_idx + 1) % 5;
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia(void) {
    ctx.julia_mode = !ctx.julia_mode;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_adjust_iterations(int diff) {
    ctx.max_iterations += diff;
    if (ctx.max_iterations < 10) ctx.max_iterations = 10;
    if (ctx.max_iterations > 10000) ctx.max_iterations = 10000;
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_resolution(int w, int h) {
    (void)w; (void)h;
}

EMSCRIPTEN_KEEPALIVE
void wasm_cancel_zoom(void) {
    ctx.is_panning = 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_zoom_at(float cx, float cy, float factor) {
    if (ctx.history_count < MAX_HISTORY_SIZE) {
        ctx.history[ctx.history_count++] = ctx.view;
    }
    
    float sw = sapp_widthf();
    float sh = sapp_heightf();
    if (sw <= 0 || sh <= 0) return;

    double aspect = (double)sw / sh;
    double re = ctx.view.center_re + (cx / sw - 0.5) * ctx.view.zoom * aspect;
    double im = ctx.view.center_im + (0.5 - cy / sh) * ctx.view.zoom;
    
    ctx.view.zoom *= factor;
    
    ctx.view.center_re = re - (cx / sw - 0.5) * ctx.view.zoom * aspect;
    ctx.view.center_im = im - (0.5 - cy / sh) * ctx.view.zoom;
    
    ctx.needs_redraw = 1;
}

#endif

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){ 
        .init_cb = init, 
        .frame_cb = frame, 
        .cleanup_cb = cleanup, 
        .event_cb = event, 
        .width = 1024, 
        .height = 768, 
        .window_title = "mandelbrot web" 
    };
}