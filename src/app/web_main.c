#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include "config.h"
#include "core_math.h"
#include "renderer.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "tour.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <GLES3/gl3.h>

EM_JS(void, update_debug_info_js, (int gpu_mode, int julia_mode, int max_iters, double zoom, double center_re, double center_im, int palette_idx, int tour_phase, double julia_re, double julia_im), {
    if (typeof updateDebugInfo === 'function') {
        updateDebugInfo(gpu_mode, julia_mode, max_iters, zoom, center_re, center_im, palette_idx, tour_phase, julia_re, julia_im);
    }
});

EM_JS(void, update_zoom_box_js, (int is_zooming, int x, int y, int w, int h), {
    if (typeof updateZoomBox === 'function') {
        updateZoomBox(is_zooming, x, y, w, h);
    }
});

EM_JS(void, download_screenshot_js, (uint32_t* ptr, int w, int h), {
    if (typeof downloadScreenshotData === 'function') {
        downloadScreenshotData(ptr, w, h, HEAPU8);
    }
});
#else
#define update_debug_info_js(...)
#define update_zoom_box_js(...)
#endif

static void slog_func(const char* tag, uint32_t log_level, uint32_t log_item_id,
                      const char* message_or_null, uint32_t line_nr, const char* filename_or_null,
                      void* user_data) {
    (void)tag;
    (void)log_level;
    (void)log_item_id;
    if (message_or_null) {
        printf("[sokol][%d] %s (line: %u, file: %s)\n", log_level, message_or_null, line_nr,
               filename_or_null ? filename_or_null : "unknown");
    }
}

typedef struct {
    float center_hi[2]; /* high bits of double center */
    float center_lo[2]; /* low bits (residual) for extra precision */
    float zoom;
    float iters;
    float aspect;
    float julia_c[2];
    float is_julia;
    float palette;
} params_t;

typedef struct {
    sg_pipeline pip_cpu, pip_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t* pixels;
    int win_w, win_h;

    ViewState view;
    ViewState history[MAX_HISTORY_SIZE];
    int history_count;

    TourState m_tour;
    int julia_mode, gpu_mode;
    complex_t julia_c;
    int max_iterations, palette_idx;
    int needs_redraw;
    int screenshot_requested;
    int is_panning, is_zooming;
    int last_mouse_x, last_mouse_y;
    int mouse_x, mouse_y;
    struct {
        int x, y, w, h;
    } zoom_rect;
} GlobalCtx;

static GlobalCtx ctx;

#include "shaders.h"

static void init(void) {
    sg_setup(&(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
    stm_setup();

    ctx.win_w = sapp_width();
    ctx.win_h = sapp_height();
    if (ctx.win_w <= 0) ctx.win_w = 1024;
    if (ctx.win_h <= 0) ctx.win_h = 768;

    ctx.pixels = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * 4);
    if (!ctx.pixels) {
        printf("[sokol] fatal: failed to allocate pixel buffer\n");
    }

    float verts[] = {-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,  1.0f, 0.0f,
                     1.0f,  -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f};
    ctx.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});

    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    ctx.bind.index_buffer =
        sg_make_buffer(&(sg_buffer_desc){.usage = {.index_buffer = true}, .data = SG_RANGE(idx)});

    ctx.img = sg_make_image(&(sg_image_desc){.width = ctx.win_w,
                                             .height = ctx.win_h,
                                             .pixel_format = SG_PIXELFORMAT_RGBA8,
                                             .usage = {.dynamic_update = true}});
    ctx.img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.img});
    ctx.smp = sg_make_sampler(
        &(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR, .mag_filter = SG_FILTER_LINEAR});

    ctx.bind.views[0] = ctx.img_view;
    ctx.bind.samplers[0] = ctx.smp;

    sg_shader shd_cpu = sg_make_shader(
        &(sg_shader_desc){.attrs[0].glsl_name = "pos",
                          .attrs[1].glsl_name = "uv_in",
                          .vertex_func.source = vs_src,
                          .fragment_func.source = fs_cpu_src,
                          .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
                          .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
                          .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                                       .view_slot = 0,
                                                       .sampler_slot = 0,
                                                       .glsl_name = "tex"}});

    ctx.pip_cpu = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = shd_cpu,
                            .layout = {.attrs[0] = {.format = SG_VERTEXFORMAT_FLOAT2},
                                       .attrs[1] = {.format = SG_VERTEXFORMAT_FLOAT2}},
                            .index_type = SG_INDEXTYPE_UINT16});

    sg_shader shd_gpu = sg_make_shader(&(sg_shader_desc){
        .attrs[0].glsl_name = "pos",
        .attrs[1].glsl_name = "uv_in",
        .vertex_func.source = vs_src,
        .fragment_func.source = fs_gpu_src,
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(params_t),
            .glsl_uniforms = {{.glsl_name = "u_center_hi", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_center_lo", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_zoom", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_iters", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_aspect", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_julia_c", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_is_julia", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_palette", .type = SG_UNIFORMTYPE_FLOAT}}}});

    ctx.pip_gpu = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = shd_gpu,
                            .layout = {.attrs[0] = {.format = SG_VERTEXFORMAT_FLOAT2},
                                       .attrs[1] = {.format = SG_VERTEXFORMAT_FLOAT2}},
                            .index_type = SG_INDEXTYPE_UINT16});

    ctx.pass_action = (sg_pass_action){
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0, 0, 0, 1}}};

    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.julia_c = (complex_t){-0.8, 0.156};
    ctx.max_iterations = DEFAULT_ITERATIONS;
    ctx.needs_redraw = 1;

#if defined(FORCE_GPU_MODE)
    ctx.gpu_mode = 1;
#else
    ctx.gpu_mode = 1;
#endif

    init_renderer(ctx.max_iterations, 0);
}

static void frame(void) {
    if (ctx.m_tour.phase != TOUR_IDLE) {
        if (ctx.julia_mode) {
            /* julia tour: animate julia_c through the classic parametric family
             * c = r*e^(it) which sweeps through many beautiful julia sets */
            double t = stm_ms(stm_now()) * 0.0006;
            double r = 0.7885;
            ctx.julia_c.re = r * cos(t);
            ctx.julia_c.im = r * sin(t);
        } else {
            update_tour(&ctx.m_tour, &ctx.view, (uint32_t)stm_ms(stm_now()));
        }
        ctx.needs_redraw = 1;
    }

    if (!ctx.gpu_mode && ctx.needs_redraw) {
        double aspect = (double)ctx.win_w / ctx.win_h;
        double rmin = ctx.view.center_re - (ctx.view.zoom * aspect) / 2;
        double rmax = ctx.view.center_re + (ctx.view.zoom * aspect) / 2;
        /* pass im_max as first arg (y=0 → high im) to match GPU y-up orientation */
        double im_top = ctx.view.center_im + ctx.view.zoom / 2;
        double im_bot = ctx.view.center_im - ctx.view.zoom / 2;

        if (ctx.julia_mode) {
            render_julia_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                  im_top, im_bot, ctx.julia_c, ctx.max_iterations);
        } else {
            render_mandelbrot_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                       im_top, im_bot, ctx.max_iterations);
        }

        sg_update_image(ctx.img, &(sg_image_data){
                                     .mip_levels[0] = {.ptr = ctx.pixels,
                                                       .size = (size_t)ctx.win_w * ctx.win_h * 4}});
        ctx.needs_redraw = 0;
    }

    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});

    sg_pipeline cur_pip = ctx.gpu_mode ? ctx.pip_gpu : ctx.pip_cpu;
    if (sg_query_pipeline_state(cur_pip) == SG_RESOURCESTATE_VALID) {
        sg_apply_pipeline(cur_pip);
        sg_apply_bindings(&ctx.bind);
        if (ctx.gpu_mode) {
            /* split double center into hi+lo floats for better zoom precision */
            float chi_re = (float)ctx.view.center_re;
            float chi_im = (float)ctx.view.center_im;
            params_t params = {
                .center_hi = {chi_re, chi_im},
                .center_lo = {(float)(ctx.view.center_re - chi_re),
                              (float)(ctx.view.center_im - chi_im)},
                .zoom = (float)ctx.view.zoom,
                .iters = (float)ctx.max_iterations,
                .aspect = (float)ctx.win_w / ctx.win_h,
                .julia_c = {(float)ctx.julia_c.re, (float)ctx.julia_c.im},
                .is_julia = ctx.julia_mode ? 1.0f : 0.0f,
                .palette = (float)ctx.palette_idx};
            sg_apply_uniforms(0, &SG_RANGE(params));
        }
        sg_draw(0, 6, 1);
        if (ctx.screenshot_requested && ctx.gpu_mode) {
            int w = ctx.win_w;
            int h = ctx.win_h;
            uint32_t* temp_pixels = (uint32_t*)malloc(w * h * 4);
            if (temp_pixels) {
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels);
                uint32_t* row_buf = (uint32_t*)malloc(w * 4);
                if (row_buf) {
                    for (int y = 0; y < h / 2; y++) {
                        uint32_t* r1 = temp_pixels + y * w;
                        uint32_t* r2 = temp_pixels + (h - 1 - y) * w;
                        memcpy(row_buf, r1, w * 4);
                        memcpy(r1, r2, w * 4);
                        memcpy(r2, row_buf, w * 4);
                    }
                    free(row_buf);
                }
                download_screenshot_js(temp_pixels, w, h);
                free(temp_pixels);
            }
            ctx.screenshot_requested = 0;
        }
    }

    sg_end_pass();
    sg_commit();

    if (ctx.screenshot_requested && !ctx.gpu_mode) {
        int w = ctx.win_w;
        int h = ctx.win_h;
        uint32_t* temp_pixels = (uint32_t*)malloc(w * h * 4);
        if (temp_pixels) {
            memcpy(temp_pixels, ctx.pixels, w * h * 4);
            download_screenshot_js(temp_pixels, w, h);
            free(temp_pixels);
        }
        ctx.screenshot_requested = 0;
    }
    update_debug_info_js(ctx.gpu_mode, ctx.julia_mode, ctx.max_iterations, ctx.view.zoom, ctx.view.center_re, ctx.view.center_im, ctx.palette_idx, ctx.m_tour.phase, ctx.julia_c.re, ctx.julia_c.im);
}

static void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
            ctx.is_panning = 1;
            ctx.last_mouse_x = (int)ev->mouse_x;
            ctx.last_mouse_y = (int)ev->mouse_y;
        } else if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            ctx.is_zooming = 1;
            ctx.zoom_rect.x = (int)ev->mouse_x;
            ctx.zoom_rect.y = (int)ev->mouse_y;
            ctx.zoom_rect.w = 0;
            ctx.zoom_rect.h = 0;
            update_zoom_box_js(1, ctx.zoom_rect.x, ctx.zoom_rect.y, 0, 0);
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP) {
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
            ctx.is_panning = 0;
        } else if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            if (ctx.is_zooming && ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
                if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
                double aspect = (double)ctx.win_w / ctx.win_h;
                double re_pp = (ctx.view.zoom * aspect) / ctx.win_w;
                double im_pp = ctx.view.zoom / ctx.win_h;
                int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
                int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
                int zw = abs(ctx.zoom_rect.w);
                int zh = abs(ctx.zoom_rect.h);
                double re_min = ctx.view.center_re - (ctx.view.zoom * aspect) / 2.0;
                double im_max = ctx.view.center_im + ctx.view.zoom / 2.0;
                ctx.view.center_re = re_min + (zx + zw / 2.0) * re_pp;
                /* Y-up: screen y=0 is top=high im, so invert y */
                ctx.view.center_im = im_max - (zy + zh / 2.0) * im_pp;
                ctx.view.zoom = fmax(zw * re_pp, zh * im_pp);
                ctx.needs_redraw = 1;
            }
            ctx.is_zooming = 0;
            update_zoom_box_js(0, 0, 0, 0, 0);
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        ctx.mouse_x = (int)ev->mouse_x;
        ctx.mouse_y = (int)ev->mouse_y;
        if (ctx.is_panning) {
            double aspect = (double)ctx.win_w / ctx.win_h;
            ctx.view.center_re -= (ev->mouse_x - ctx.last_mouse_x) * (ctx.view.zoom * aspect) / ctx.win_w;
            /* Y-up grab model: dragging down (delta_y > 0) moves image down
             * which means center_im must increase so the grabbed point follows */
            ctx.view.center_im += (ev->mouse_y - ctx.last_mouse_y) * ctx.view.zoom / ctx.win_h;
            ctx.last_mouse_x = (int)ev->mouse_x;
            ctx.last_mouse_y = (int)ev->mouse_y;
            ctx.needs_redraw = 1;
        } else if (ctx.is_zooming) {
            ctx.zoom_rect.w = (int)ev->mouse_x - ctx.zoom_rect.x;
            ctx.zoom_rect.h = (int)ev->mouse_y - ctx.zoom_rect.y;
            int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
            int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
            update_zoom_box_js(1, zx, zy, abs(ctx.zoom_rect.w), abs(ctx.zoom_rect.h));
        } else if (ctx.julia_mode && ctx.m_tour.phase == TOUR_IDLE) {
            /* Y-up: top of screen = high im */
            ctx.julia_c.re = ctx.view.center_re +
                ((double)ctx.mouse_x / ctx.win_w - 0.5) * ctx.view.zoom * ((double)ctx.win_w / ctx.win_h);
            ctx.julia_c.im = ctx.view.center_im +
                (0.5 - (double)ctx.mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        double zoom_factor = (ev->scroll_y > 0) ? 0.9 : 1.1;
        if (ev->scroll_y != 0.0f) {
            if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
            double aspect = (double)ctx.win_w / ctx.win_h;
            /* Y-up: top = high im; scroll zoom at cursor using Y-up mapping */
            double mouse_re = ctx.view.center_re +
                ((double)ev->mouse_x / ctx.win_w - 0.5) * ctx.view.zoom * aspect;
            double mouse_im = ctx.view.center_im +
                (0.5 - (double)ev->mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.view.zoom *= zoom_factor;
            ctx.view.center_re = mouse_re - ((double)ev->mouse_x / ctx.win_w - 0.5) * ctx.view.zoom * aspect;
            ctx.view.center_im = mouse_im - (0.5 - (double)ev->mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (ev->key_code == SAPP_KEYCODE_G) {
            ctx.gpu_mode = !ctx.gpu_mode;
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_R) {
            ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_UP) {
            ctx.max_iterations += 10;
            if (ctx.max_iterations > 10000) ctx.max_iterations = 10000;
            init_renderer(ctx.max_iterations, ctx.palette_idx);
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_DOWN) {
            ctx.max_iterations -= 10;
            if (ctx.max_iterations < 10) ctx.max_iterations = 10;
            init_renderer(ctx.max_iterations, ctx.palette_idx);
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        ctx.win_w = (int)ev->framebuffer_width;
        ctx.win_h = (int)ev->framebuffer_height;
        if (ctx.pixels) free(ctx.pixels);
        ctx.pixels = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * 4);
        if (!ctx.pixels) {
            printf("[sokol] fatal: failed to allocate pixel buffer\n");
            return;
        }

        sg_destroy_image(ctx.img);
        ctx.img = sg_make_image(&(sg_image_desc){.width = ctx.win_w,
                                                 .height = ctx.win_h,
                                                 .pixel_format = SG_PIXELFORMAT_RGBA8,
                                                 .usage = {.dynamic_update = true}});
        sg_destroy_view(ctx.img_view);
        ctx.img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.img});
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
    ctx.palette_idx = (ctx.palette_idx + 1) % 6;
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
    (void)w;
    (void)h;
}

EMSCRIPTEN_KEEPALIVE
void wasm_cancel_zoom(void) {
    ctx.is_panning = 0;
    ctx.is_zooming = 0;
    update_zoom_box_js(0, 0, 0, 0, 0);
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_down(float x, float y, int button) {
    if (button == 0) { // Left
        ctx.is_zooming = 1;
        ctx.zoom_rect.x = (int)x;
        ctx.zoom_rect.y = (int)y;
        ctx.zoom_rect.w = 0;
        ctx.zoom_rect.h = 0;
        update_zoom_box_js(1, ctx.zoom_rect.x, ctx.zoom_rect.y, 0, 0);
    } else if (button == 1) { // Right (panning)
        ctx.is_panning = 1;
        ctx.last_mouse_x = (int)x;
        ctx.last_mouse_y = (int)y;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_move(float x, float y) {
    ctx.mouse_x = (int)x;
    ctx.mouse_y = (int)y;
    if (ctx.is_panning) {
        double aspect = (double)ctx.win_w / ctx.win_h;
        ctx.view.center_re -= (x - ctx.last_mouse_x) * (ctx.view.zoom * aspect) / ctx.win_w;
        ctx.view.center_im += (y - ctx.last_mouse_y) * ctx.view.zoom / ctx.win_h;
        ctx.last_mouse_x = (int)x;
        ctx.last_mouse_y = (int)y;
        ctx.needs_redraw = 1;
    } else if (ctx.is_zooming) {
        ctx.zoom_rect.w = (int)x - ctx.zoom_rect.x;
        ctx.zoom_rect.h = (int)y - ctx.zoom_rect.y;
        int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
        int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
        update_zoom_box_js(1, zx, zy, abs(ctx.zoom_rect.w), abs(ctx.zoom_rect.h));
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_up(float x, float y, int button) {
    (void)x; (void)y;
    if (button == 0) { // Left
        if (ctx.is_zooming && ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
            if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
            double aspect = (double)ctx.win_w / ctx.win_h;
            double re_pp = (ctx.view.zoom * aspect) / ctx.win_w;
            double im_pp = ctx.view.zoom / ctx.win_h;
            int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
            int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
            int zw = abs(ctx.zoom_rect.w);
            int zh = abs(ctx.zoom_rect.h);
            double re_min = ctx.view.center_re - (ctx.view.zoom * aspect) / 2.0;
            double im_max = ctx.view.center_im + ctx.view.zoom / 2.0;
            ctx.view.center_re = re_min + (zx + zw / 2.0) * re_pp;
            ctx.view.center_im = im_max - (zy + zh / 2.0) * im_pp;
            ctx.view.zoom = fmax(zw * re_pp, zh * im_pp);
            ctx.needs_redraw = 1;
        }
        ctx.is_zooming = 0;
        update_zoom_box_js(0, 0, 0, 0, 0);
    } else if (button == 1) { // Right
        ctx.is_panning = 0;
    }
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

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_gpu(void) {
    ctx.gpu_mode = !ctx.gpu_mode;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_request_screenshot(void) {
    ctx.screenshot_requested = 1;
}

#endif

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){.init_cb = init,
                       .frame_cb = frame,
                       .cleanup_cb = cleanup,
                       .event_cb = event,
                       .width = 1024,
                       .height = 768,
                       .window_title = "mandelbrot web"};
}