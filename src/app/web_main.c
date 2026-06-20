/* web_main.c
 *
 * webassembly entry point using emscripten and the sokol framework.
 * manages the bridge between C/WASM logic and the browser's javascript/webgl layer.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#ifndef SOKOL_GLES3
#define SOKOL_GLES3  // force gles3 for webgl2 support
#endif
#else
#define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL

#include "color.h"
#include "config.h"
#include "core_math.h"
#include "ini_config.h"
#include "renderer.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "tour.h"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#include <emscripten.h>

// clang-format off
/* javascript interop: updates the web hud with engine telemetry.
 * passes internal state directly to a globally defined js function. */
EM_JS(void, update_debug_info_js,
      (int gpu_mode, int julia_mode, int burning_ship_mode, int max_iters, double zoom,
       double center_re, double center_im, int palette_idx, int tour_phase, double julia_re,
       double julia_im, int high_precision, int tour_target_idx, int tour_total_targets,
       double tour_target_re, double tour_target_im),
      {
          if (typeof updateDebugInfo === 'function') {
              updateDebugInfo(gpu_mode, julia_mode, burning_ship_mode, max_iters, zoom, center_re,
                              center_im, palette_idx, tour_phase, julia_re, julia_im,
                              high_precision, tour_target_idx, tour_total_targets, tour_target_re,
                              tour_target_im);
          }
      });

/* javascript interop: synchronizes the visual zoom selection box. */
EM_JS(void, update_zoom_box_js, (int is_zooming, int x, int y, int w, int h), {
    if (typeof updateZoomBox === 'function') {
        updateZoomBox(is_zooming, x, y, w, h);
    }
});

/* javascript interop: triggers a browser download of the captured frame.
 * utilizes the browser's native blob and url object APIs. */
EM_JS(void, download_screenshot_js, (uint32_t* ptr, int w, int h), {
    if (typeof downloadScreenshotData === 'function') {
        downloadScreenshotData(ptr, w, h, HEAPU8);
    }
});
// clang-format on
#else
#define update_debug_info_js(...)
#define update_zoom_box_js(...)
#endif

// internal logger for graphics api events
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

// uniform block layout — must match the glsl declaration in shaders.h exactly.
typedef struct {
    float center_hi[2];
    float center_lo[2];
    float julia_c_hi[2];
    float julia_c_lo[2];
    float zoom;
    float iters;
    float aspect;
    float fractal_type;
    float palette;
    float high_precision;
    float zero;  // padding/optimization barrier
} params_t;

// application state context
typedef struct {
    sg_pipeline pip_cpu, pip_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t* pixels;  // staging buffer for cpu-to-texture uploads
    int win_w, win_h;

#define JULIA_ZOOM 4.0

    // navigation state
    ViewState view;
    ViewState history[MAX_HISTORY_SIZE];
    int history_count;

    // runtime flags
    TourState m_tour;
    int julia_mode, burning_ship_mode, gpu_mode, high_precision_mode;
    complex_t julia_c;
    int julia_c_locked;

    int max_iterations, palette_idx;
    int needs_redraw;
    int screenshot_requested;

    // interaction tracking
    int is_panning, is_zooming;
    int last_mouse_x, last_mouse_y;
    int mouse_x, mouse_y;
    struct {
        int x, y, w, h;
    } zoom_rect;

    // session persistence
    struct {
        int active;
        ViewState mandelbrot_view;
    } julia_session;
} GlobalCtx;

static GlobalCtx ctx;

#include "shaders.h"

// initializes graphics pipeline and application defaults
static void init(void) {
    sg_setup(&(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
    stm_setup();

    ctx.win_w = sapp_width();
    ctx.win_h = sapp_height();
    if (ctx.win_w <= 0) ctx.win_w = 1024;
    if (ctx.win_h <= 0) ctx.win_h = 768;

    ctx.pixels = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * 4);

    // geometry: full-screen triangle fan (rectangle)
    float verts[] = {-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  1.0f,  1.0f, 0.0f,
                     1.0f,  -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f};
    ctx.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});

    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    ctx.bind.index_buffer =
        sg_make_buffer(&(sg_buffer_desc){.usage = {.index_buffer = true}, .data = SG_RANGE(idx)});

    // dynamic image for cpu-mode rendering
    ctx.img = sg_make_image(&(sg_image_desc){.width = ctx.win_w,
                                             .height = ctx.win_h,
                                             .pixel_format = SG_PIXELFORMAT_RGBA8,
                                             .usage = {.dynamic_update = true}});
    ctx.img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.img});
    ctx.smp = sg_make_sampler(
        &(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR, .mag_filter = SG_FILTER_LINEAR});

    ctx.bind.views[0] = ctx.img_view;
    ctx.bind.samplers[0] = ctx.smp;

    // build shaders and pipelines
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
                              {.glsl_name = "u_julia_c_hi", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_julia_c_lo", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_zoom", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_iters", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_aspect", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_fractal_type", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_palette", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_high_precision", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_zero", .type = SG_UNIFORMTYPE_FLOAT}}}});

    ctx.pip_gpu = sg_make_pipeline(
        &(sg_pipeline_desc){.shader = shd_gpu,
                            .layout = {.attrs[0] = {.format = SG_VERTEXFORMAT_FLOAT2},
                                       .attrs[1] = {.format = SG_VERTEXFORMAT_FLOAT2}},
                            .index_type = SG_INDEXTYPE_UINT16});

    ctx.pass_action = (sg_pass_action){
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0, 0, 0, 1}}};

    // initialize or restore state
    if (ctx.view.zoom == 0.0) {
        ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    }
    if (ctx.julia_c.re == 0.0 && ctx.julia_c.im == 0.0) {
        ctx.julia_c = (complex_t){-0.8, 0.156};
    }
    if (ctx.max_iterations == 0) {
        ctx.max_iterations = DEFAULT_ITERATIONS;
    }

    ctx.needs_redraw = 1;
    ctx.gpu_mode = 1;

    init_renderer(ctx.max_iterations, DEFAULT_PALETTE);
    ctx.palette_idx = DEFAULT_PALETTE;
}

// main execution loop dispatched by the browser's requestAnimationFrame
static void frame(void) {
    if (ctx.m_tour.phase != TOUR_IDLE) {
        if (ctx.julia_mode) {
            // web-exclusive julia animation: circular orbit
            double t = stm_ms(stm_now()) * 0.0006;
            double r = 0.7885;
            ctx.julia_c.re = r * cos(t);
            ctx.julia_c.im = r * sin(t);
        } else {
            update_tour(&ctx.m_tour, &ctx.view, (uint32_t)stm_ms(stm_now()), ctx.burning_ship_mode);
        }
        ctx.needs_redraw = 1;
    }

    // cpu path: render in workers and upload to webgl texture
    if (!ctx.gpu_mode && ctx.needs_redraw) {
        double aspect = (double)ctx.win_w / ctx.win_h;
        double rmin = ctx.view.center_re - (ctx.view.zoom * aspect) / 2;
        double rmax = ctx.view.center_re + (ctx.view.zoom * aspect) / 2;
        double im_top = ctx.view.center_im + ctx.view.zoom / 2;
        double im_bot = ctx.view.center_im - ctx.view.zoom / 2;

        if (ctx.julia_mode) {
            render_julia_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                  im_top, im_bot, ctx.julia_c, ctx.max_iterations);
        } else if (ctx.burning_ship_mode) {
            render_burning_ship_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin,
                                         rmax, im_top, im_bot, ctx.max_iterations);
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
            float chi_re = (float)ctx.view.center_re;
            float chi_im = (float)ctx.view.center_im;
            float jhi_re = (float)ctx.julia_c.re;
            float jhi_im = (float)ctx.julia_c.im;
            params_t params = {
                .center_hi = {chi_re, chi_im},
                .center_lo = {(float)(ctx.view.center_re - chi_re),
                              (float)(ctx.view.center_im - chi_im)},
                .julia_c_hi = {jhi_re, jhi_im},
                .julia_c_lo = {(float)(ctx.julia_c.re - jhi_re), (float)(ctx.julia_c.im - jhi_im)},
                .zoom = (float)ctx.view.zoom,
                .iters = (float)ctx.max_iterations,
                .aspect = (float)ctx.win_w / ctx.win_h,
                .fractal_type = ctx.julia_mode ? 1.0f : (ctx.burning_ship_mode ? 2.0f : 0.0f),
                .palette = (float)ctx.palette_idx,
                .high_precision = ctx.high_precision_mode ? 1.0f : 0.0f,
                .zero = 0.0f};
            sg_apply_uniforms(0, &SG_RANGE(params));
        }
        sg_draw(0, 6, 1);

        // deferred gpu capture logic
        if (ctx.screenshot_requested && ctx.gpu_mode) {
            int w = ctx.win_w;
            int h = ctx.win_h;
            uint32_t* temp_pixels = (uint32_t*)malloc(w * h * 4);
            if (temp_pixels) {
                // webgl coordinate system is y-down relative to canvas; vertical flip required
                glReadPixels(0, 0, ctx.win_w, ctx.win_h, GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels);
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

    // handle cpu capture separately (already in ram)
    if (ctx.screenshot_requested && !ctx.gpu_mode) {
        int w = ctx.win_w, h = ctx.win_h;
        uint32_t* temp_pixels = (uint32_t*)malloc(w * h * 4);
        if (temp_pixels) {
            memcpy(temp_pixels, ctx.pixels, w * h * 4);
            download_screenshot_js(temp_pixels, w, h);
            free(temp_pixels);
        }
        ctx.screenshot_requested = 0;
    }

    // sync state with the external javascript hud
    int tour_idx = get_tour_target_idx(&ctx.m_tour);
    int tour_total = get_num_tour_targets(ctx.burning_ship_mode);
    double tour_re = get_tour_target_re(&ctx.m_tour, ctx.burning_ship_mode);
    double tour_im = get_tour_target_im(&ctx.m_tour, ctx.burning_ship_mode);

    update_debug_info_js(ctx.gpu_mode, ctx.julia_mode, ctx.burning_ship_mode, ctx.max_iterations,
                         ctx.view.zoom, ctx.view.center_re, ctx.view.center_im, ctx.palette_idx,
                         ctx.m_tour.phase, ctx.julia_c.re, ctx.julia_c.im, ctx.high_precision_mode,
                         tour_idx, tour_total, tour_re, tour_im);
}

// maps browser-driven input events to engine state
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
            ctx.zoom_rect.w = ctx.zoom_rect.h = 0;
            update_zoom_box_js(1, ctx.zoom_rect.x, ctx.zoom_rect.y, 0, 0);
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP) {
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
            ctx.is_panning = 0;
        } else if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            if (ctx.is_zooming && ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
                if (ctx.history_count < MAX_HISTORY_SIZE)
                    ctx.history[ctx.history_count++] = ctx.view;
                double aspect = (double)ctx.win_w / ctx.win_h;
                double re_pp = (ctx.view.zoom * aspect) / ctx.win_w;
                double im_pp = ctx.view.zoom / ctx.win_h;
                int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
                int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
                int zw = abs(ctx.zoom_rect.w), zh = abs(ctx.zoom_rect.h);
                double re_min = ctx.view.center_re - (ctx.view.zoom * aspect) / 2.0;
                double im_max = ctx.view.center_im + ctx.view.zoom / 2.0;
                ctx.view.center_re = re_min + (zx + zw / 2.0) * re_pp;
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
            ctx.view.center_re -=
                (ev->mouse_x - ctx.last_mouse_x) * (ctx.view.zoom * aspect) / ctx.win_w;
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
        } else if (ctx.julia_mode && !ctx.julia_c_locked && ctx.m_tour.phase == TOUR_IDLE) {
            ctx.julia_c.re = ctx.view.center_re + ((double)ctx.mouse_x / ctx.win_w - 0.5) *
                                                      ctx.view.zoom *
                                                      ((double)ctx.win_w / ctx.win_h);
            ctx.julia_c.im =
                ctx.view.center_im + (0.5 - (double)ctx.mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        double zoom_factor = (ev->scroll_y > 0) ? 0.9 : 1.1;
        if (ev->scroll_y != 0.0f) {
            if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
            double aspect = (double)ctx.win_w / ctx.win_h;
            double mre = ctx.view.center_re +
                         ((double)ev->mouse_x / ctx.win_w - 0.5) * ctx.view.zoom * aspect;
            double mim =
                ctx.view.center_im + (0.5 - (double)ev->mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.view.zoom *= zoom_factor;
            ctx.view.center_re =
                mre - ((double)ev->mouse_x / ctx.win_w - 0.5) * ctx.view.zoom * aspect;
            ctx.view.center_im = mim - (0.5 - (double)ev->mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (ev->key_code == SAPP_KEYCODE_G) {
            ctx.gpu_mode = !ctx.gpu_mode;
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_E) {
            if (ctx.gpu_mode) {
                ctx.high_precision_mode = !ctx.high_precision_mode;
                ctx.needs_redraw = 1;
            }
        } else if (ev->key_code == SAPP_KEYCODE_R) {
            ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_UP) {
            ctx.max_iterations += ctx.max_iterations / 10;
            if (ctx.max_iterations > get_config_max_iterations_limit())
                ctx.max_iterations = get_config_max_iterations_limit();
            init_renderer(ctx.max_iterations, ctx.palette_idx);
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_DOWN) {
            ctx.max_iterations -= ctx.max_iterations / 10;
            if (ctx.max_iterations < 10) ctx.max_iterations = 10;
            init_renderer(ctx.max_iterations, ctx.palette_idx);
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_B) {
            ctx.burning_ship_mode = !ctx.burning_ship_mode;
            ctx.julia_mode = 0;
            ctx.m_tour.phase = TOUR_IDLE;
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        ctx.win_w = (int)ev->framebuffer_width;
        ctx.win_h = (int)ev->framebuffer_height;
        if (ctx.pixels) free(ctx.pixels);
        ctx.pixels = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * 4);
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
    cleanup_color_palette();
    sg_shutdown();
}

#if defined(__EMSCRIPTEN__)

// exported symbols for javascript control
EMSCRIPTEN_KEEPALIVE
void wasm_reset_view(void) {
    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_view(double re, double im, double zoom) {
    ctx.view.center_re = re;
    ctx.view.center_im = im;
    ctx.view.zoom = zoom;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_state(int julia_mode, double jre, double jim, int iters, int palette) {
    ctx.julia_mode = julia_mode;
    ctx.julia_c.re = jre;
    ctx.julia_c.im = jim;
    if (iters > 0) ctx.max_iterations = iters;
    if (palette >= 0) ctx.palette_idx = palette % PALETTE_COUNT;
    init_renderer(ctx.max_iterations, ctx.palette_idx);
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
    if (ctx.m_tour.phase == TOUR_IDLE)
        start_tour(&ctx.m_tour, &ctx.view);
    else
        stop_tour(&ctx.m_tour);
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_next_palette(void) {
    ctx.palette_idx = (ctx.palette_idx + 1) % PALETTE_COUNT;
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_palette(int idx) {
    if (idx >= 0 && idx < PALETTE_COUNT) {
        ctx.palette_idx = idx;
        init_renderer(ctx.max_iterations, ctx.palette_idx);
        ctx.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia_lock(void) {
    ctx.julia_c_locked = !ctx.julia_c_locked;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
int wasm_is_julia_locked(void) {
    return ctx.julia_c_locked;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia(void) {
    ctx.m_tour.phase = TOUR_IDLE;
    if (!ctx.julia_mode) {
        ctx.julia_session.mandelbrot_view = ctx.view;
        ctx.julia_session.active = 1;
        if (ctx.julia_c.re == 0.0 && ctx.julia_c.im == 0.0) ctx.julia_c = (complex_t){-0.8, 0.156};
        ctx.view = (ViewState){0.0, 0.0, JULIA_ZOOM};
        ctx.julia_mode = 1;
        ctx.history_count = 0;
    } else {
        if (ctx.julia_session.active) ctx.view = ctx.julia_session.mandelbrot_view;
        ctx.julia_mode = 0;
        ctx.history_count = 0;
    }
    ctx.burning_ship_mode = 0;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_burning_ship(void) {
    ctx.burning_ship_mode = !ctx.burning_ship_mode;
    ctx.julia_mode = 0;
    ctx.julia_session.active = 0;
    ctx.m_tour.phase = TOUR_IDLE;
    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.history_count = 0;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_adjust_iterations(int diff) {
    ctx.max_iterations += diff;
    if (ctx.max_iterations < 10) ctx.max_iterations = 10;
    if (ctx.max_iterations > get_config_max_iterations_limit())
        ctx.max_iterations = get_config_max_iterations_limit();
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
    if (button == 0) {
        ctx.is_zooming = 1;
        ctx.zoom_rect.x = (int)x;
        ctx.zoom_rect.y = (int)y;
        ctx.zoom_rect.w = ctx.zoom_rect.h = 0;
        update_zoom_box_js(1, ctx.zoom_rect.x, ctx.zoom_rect.y, 0, 0);
    } else if (button == 1) {
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
    (void)x;
    (void)y;
    if (button == 0) {
        if (ctx.is_zooming && ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
            if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
            double aspect = (double)ctx.win_w / ctx.win_h;
            double re_pp = (ctx.view.zoom * aspect) / ctx.win_w;
            double im_pp = ctx.view.zoom / ctx.win_h;
            int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
            int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
            int zw = abs(ctx.zoom_rect.w), zh = abs(ctx.zoom_rect.h);
            double re_min = ctx.view.center_re - (ctx.view.zoom * aspect) / 2.0;
            double im_max = ctx.view.center_im + ctx.view.zoom / 2.0;
            ctx.view.center_re = re_min + (zx + zw / 2.0) * re_pp;
            ctx.view.center_im = im_max - (zy + zh / 2.0) * im_pp;
            ctx.view.zoom = fmax(zw * re_pp, zh * im_pp);
            ctx.needs_redraw = 1;
        }
        ctx.is_zooming = 0;
        update_zoom_box_js(0, 0, 0, 0, 0);
    } else if (button == 1)
        ctx.is_panning = 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_zoom_at(float cx, float cy, float factor) {
    if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
    float sw = sapp_widthf(), sh = sapp_heightf();
    if (sw <= 0 || sh <= 0) return;
    double aspect = (double)sw / sh;
    double re = ctx.view.center_re + (cx / sw - 0.5) * ctx.view.zoom * aspect;
    double im = ctx.view.center_im + (0.5 - cy / sh) * ctx.view.zoom;
    double min_zoom = (ctx.gpu_mode ? 1e-15 : (ctx.high_precision_mode ? 1e-30 : 1e-15));
    double next_zoom = ctx.view.zoom * factor;
    if (next_zoom < min_zoom) factor = min_zoom / ctx.view.zoom;
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

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_precision(void) {
    ctx.high_precision_mode = !ctx.high_precision_mode;
    set_cpu_precision(ctx.high_precision_mode);
    ctx.needs_redraw = 1;
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
