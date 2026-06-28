/* web_main.c
 *
 * webassembly entry point using emscripten and the sokol framework.
 * manages the bridge between C/WASM logic and the browser's javascript layer.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#undef SOKOL_IMPL
#include "input_handler.h"
#include "tour.h"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#include <emscripten.h>

// clang-format off
// javascript interop: updates the web hud with engine telemetry.
// passes internal state directly to a globally defined js function.
EM_JS(void, update_debug_info_js,
      (int gpu_mode, int julia_mode, int base_fractal, int max_iters, double zoom,
       double center_re, double center_im, int palette_idx, int tour_phase, double julia_re,
       double julia_im, int high_precision, int tour_target_idx, int tour_total_targets,
       double tour_target_re, double tour_target_im),
      {
          if (typeof updateDebugInfo === 'function') {
              updateDebugInfo(gpu_mode, julia_mode, base_fractal, max_iters, zoom, center_re,
                              center_im, palette_idx, tour_phase, julia_re, julia_im,
                              high_precision, tour_target_idx, tour_total_targets, tour_target_re,
                              tour_target_im);
          }
      })

// javascript interop: synchronizes the visual zoom selection box.
EM_JS(void, update_zoom_box_js, (int is_zooming, int x, int y, int w, int h), {
    if (typeof updateZoomBox === 'function') {
        updateZoomBox(is_zooming, x, y, w, h);
    }
})

// javascript interop: triggers a browser download of the captured frame.
// utilizes the browser's native blob and url object APIs.
EM_JS(void, download_screenshot_js, (uint32_t* ptr, int w, int h), {
    if (typeof downloadScreenshotData === 'function') {
        downloadScreenshotData(ptr, w, h, HEAPU8);
    }
})
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

#define JULIA_ZOOM 4.0

#include "app_state.h"

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

    AppCommonState core;
    
    // web specific runtime flags
    int gpu_mode, high_precision_mode;
    int screenshot_requested;
    RendererContext* renderer_ctx;
    
    int julia_c_locked; // javascript toggle
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
    app_state_init(&ctx.core, ctx.win_w, ctx.win_h);

    ctx.gpu_mode = 1;

    init_fractal_registry();
    ctx.renderer_ctx = init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
    if (!ctx.renderer_ctx) {
        fprintf(stderr, "error: failed to initialize renderer context\n");
        exit(1);
    }
}

static void frame(void) {
    uint32_t now = (uint32_t)stm_ms(stm_now());
    app_state_update_tours(&ctx.core, now, NULL);

    // cpu path: render in workers and upload to webgl texture
    if (!ctx.gpu_mode && ctx.core.needs_redraw) {
        precise_float aspect = (precise_float)ctx.win_w / ctx.win_h;
        precise_float rmin = ctx.core.cam.view.center_re - (ctx.core.cam.view.zoom * aspect) / 2;
        precise_float rmax = ctx.core.cam.view.center_re + (ctx.core.cam.view.zoom * aspect) / 2;
        precise_float im_top = ctx.core.cam.view.center_im + ctx.core.cam.view.zoom / 2;
        precise_float im_bot = ctx.core.cam.view.center_im - ctx.core.cam.view.zoom / 2;

        if (ctx.core.julia_mode) {
            render_julia_threaded(ctx.renderer_ctx, ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                  im_top, im_bot, ctx.core.julia_c, ctx.core.max_iterations);
        } else if (ctx.core.base_fractal) {
            render_burning_ship_threaded(ctx.renderer_ctx, ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin,
                                         rmax, im_top, im_bot, ctx.core.max_iterations);
        } else {
            render_mandelbrot_threaded(ctx.renderer_ctx, ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                       im_top, im_bot, ctx.core.max_iterations);
        }

        sg_update_image(ctx.img, &(sg_image_data){
                                     .mip_levels[0] = {.ptr = ctx.pixels,
                                                       .size = (size_t)ctx.win_w * ctx.win_h * 4}});
        ctx.core.needs_redraw = 0;
    }

    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});

    sg_pipeline cur_pip = ctx.gpu_mode ? ctx.pip_gpu : ctx.pip_cpu;
    if (sg_query_pipeline_state(cur_pip) == SG_RESOURCESTATE_VALID) {
        sg_apply_pipeline(cur_pip);
        sg_apply_bindings(&ctx.bind);
        if (ctx.gpu_mode) {
            float chi_re = (float)ctx.core.cam.view.center_re;
            float chi_im = (float)ctx.core.cam.view.center_im;
            float jhi_re = (float)ctx.core.julia_c.re;
            float jhi_im = (float)ctx.core.julia_c.im;
            params_t params = {
                .center_hi = {chi_re, chi_im},
                .center_lo = {(float)(ctx.core.cam.view.center_re - chi_re),
                              (float)(ctx.core.cam.view.center_im - chi_im)},
                .julia_c_hi = {jhi_re, jhi_im},
                .julia_c_lo = {(float)(ctx.core.julia_c.re - jhi_re), (float)(ctx.core.julia_c.im - jhi_im)},
                .zoom = (float)ctx.core.cam.view.zoom,
                .iters = (float)ctx.core.max_iterations,
                .aspect = (float)ctx.win_w / ctx.win_h,
                .fractal_type = ctx.core.julia_mode ? 1.0f : (float)ctx.core.base_fractal,
                .palette = (float)ctx.core.palette_idx,
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
    int tour_idx = get_tour_target_idx(&ctx.core.m_tour);
    int tour_total = get_num_tour_targets(ctx.core.base_fractal);
    double tour_re = get_tour_target_re(&ctx.core.m_tour, ctx.core.base_fractal);
    double tour_im = get_tour_target_im(&ctx.core.m_tour, ctx.core.base_fractal);

    update_debug_info_js(ctx.gpu_mode, ctx.core.julia_mode, ctx.core.base_fractal, ctx.core.max_iterations,
                         ctx.core.cam.view.zoom, ctx.core.cam.view.center_re, ctx.core.cam.view.center_im, ctx.core.palette_idx,
                         ctx.core.m_tour.phase, ctx.core.julia_c.re, ctx.core.julia_c.im, ctx.high_precision_mode,
                         tour_idx, tour_total, tour_re, tour_im);
}

static InputKey map_sokol_key(sapp_keycode sym) {
    switch (sym) {
        case SAPP_KEYCODE_ESCAPE: return KEY_ESCAPE;
        case SAPP_KEYCODE_Q: return KEY_Q;
        case SAPP_KEYCODE_H: return KEY_H;
        case SAPP_KEYCODE_Z: return KEY_Z;
        case SAPP_KEYCODE_R: return KEY_R;
        case SAPP_KEYCODE_P: return KEY_P;
        case SAPP_KEYCODE_0: return KEY_0;
        case SAPP_KEYCODE_1: return KEY_1;
        case SAPP_KEYCODE_2: return KEY_2;
        case SAPP_KEYCODE_3: return KEY_3;
        case SAPP_KEYCODE_4: return KEY_4;
        case SAPP_KEYCODE_5: return KEY_5;
        case SAPP_KEYCODE_6: return KEY_6;
        case SAPP_KEYCODE_7: return KEY_7;
        case SAPP_KEYCODE_8: return KEY_8;
        case SAPP_KEYCODE_9: return KEY_9;
        case SAPP_KEYCODE_UP: return KEY_UP;
        case SAPP_KEYCODE_DOWN: return KEY_DOWN;
        case SAPP_KEYCODE_N: return KEY_N;
        case SAPP_KEYCODE_E: return KEY_E;
        case SAPP_KEYCODE_G: return KEY_G;
        case SAPP_KEYCODE_F: return KEY_F;
        case SAPP_KEYCODE_J: return KEY_J;
        case SAPP_KEYCODE_B: return KEY_B;
        case SAPP_KEYCODE_S: return KEY_S;
        case SAPP_KEYCODE_X: return KEY_X;
        case SAPP_KEYCODE_V: return KEY_V;
        case SAPP_KEYCODE_M: return KEY_M;
        case SAPP_KEYCODE_L: return KEY_L;
        case SAPP_KEYCODE_T: return KEY_T;
        case SAPP_KEYCODE_LEFT_BRACKET: return KEY_LEFT_BRACKET;
        case SAPP_KEYCODE_RIGHT_BRACKET: return KEY_RIGHT_BRACKET;
        case SAPP_KEYCODE_F5: return KEY_F5;
        default: return KEY_UNKNOWN;
    }
}

static void event(const sapp_event* ev) {
    uint32_t now = (uint32_t)stm_ms(stm_now());
    AppInputEvent ie = {0};
    int handled = 0;

    switch (ev->type) {
        case SAPP_EVENTTYPE_RESIZED:
            if (ev->framebuffer_width > 0 && ev->framebuffer_height > 0) {
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
                camera_resize(&ctx.core.cam, ctx.win_w, ctx.win_h);
                ctx.core.needs_redraw = 1;
            }
            handled = 1;
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
            ie.type = INPUT_KEY_DOWN;
            ie.key = map_sokol_key(ev->key_code);
            ie.mod_shift = (ev->modifiers & SAPP_MODIFIER_SHIFT) ? 1 : 0;
            ie.mod_ctrl = (ev->modifiers & SAPP_MODIFIER_CTRL) ? 1 : 0;
            break;

        case SAPP_EVENTTYPE_KEY_UP:
            ie.type = INPUT_KEY_UP;
            ie.key = map_sokol_key(ev->key_code);
            ie.mod_shift = (ev->modifiers & SAPP_MODIFIER_SHIFT) ? 1 : 0;
            ie.mod_ctrl = (ev->modifiers & SAPP_MODIFIER_CTRL) ? 1 : 0;
            break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            ie.type = INPUT_MOUSE_SCROLL;
            ie.scroll_y = ev->scroll_y;
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN:
            ie.type = INPUT_MOUSE_DOWN;
            ie.mouse_btn = (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            break;

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            ie.type = INPUT_MOUSE_MOVE;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            ie.type = INPUT_MOUSE_UP;
            ie.mouse_btn = (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            break;

        default:
            handled = 1;
            break;
    }

    if (!handled) {
        InputAction action = app_handle_input(&ctx.core, &ie, now);
        switch (action) {
            case ACTION_TOGGLE_GPU:
                ctx.gpu_mode = !ctx.gpu_mode;
                ctx.core.needs_redraw = 1;
                break;
            case ACTION_TOGGLE_PRECISION:
                if (ctx.gpu_mode) {
                    ctx.high_precision_mode = !ctx.high_precision_mode;
                    ctx.core.needs_redraw = 1;
                }
                break;
            case ACTION_QUIT:
                // do nothing on web
                break;
            default:
                if (ie.type == INPUT_KEY_DOWN && ie.key == KEY_S) {
                    ctx.screenshot_requested = 1;
                    ctx.core.needs_redraw = 1;
                }
                break;
        }

        // handle js zoom box 
        if (ctx.core.cam.is_zooming) {
            int zx = ctx.core.cam.zoom_rect.w > 0 ? ctx.core.cam.zoom_rect.x : ctx.core.cam.zoom_rect.x + ctx.core.cam.zoom_rect.w;
            int zy = ctx.core.cam.zoom_rect.h > 0 ? ctx.core.cam.zoom_rect.y : ctx.core.cam.zoom_rect.y + ctx.core.cam.zoom_rect.h;
            update_zoom_box_js(1, zx, zy, abs(ctx.core.cam.zoom_rect.w), abs(ctx.core.cam.zoom_rect.h));
        } else {
            update_zoom_box_js(0, 0, 0, 0, 0);
        }

        // julia hover picking for web mode
        if (ie.type == INPUT_MOUSE_MOVE && ctx.core.julia_mode && !ctx.julia_c_locked && ctx.core.m_tour.phase == TOUR_IDLE && !ctx.core.cam.is_panning && !ctx.core.cam.is_zooming) {
            app_state_get_mouse_coord(&ctx.core, ie.mouse_x, ie.mouse_y, &ctx.core.julia_c.re, &ctx.core.julia_c.im);
            ctx.core.needs_redraw = 1;
        }
    }
}

static void cleanup(void) {
    free(ctx.pixels);
    cleanup_renderer(ctx.renderer_ctx);
    cleanup_color_palette();
    sg_shutdown();
}

#if defined(__EMSCRIPTEN__)

// exported symbols for javascript control
EMSCRIPTEN_KEEPALIVE
void wasm_reset_view(void) {
    app_state_reset(&ctx.core, NULL);
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_view(double re, double im, double zoom) {
    ctx.core.cam.view.center_re = re;
    ctx.core.cam.view.center_im = im;
    ctx.core.cam.view.zoom = zoom;
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_state(int julia_mode, double jre, double jim, int iters, int palette) {
    ctx.core.julia_mode = julia_mode;
    ctx.core.julia_c.re = jre;
    ctx.core.julia_c.im = jim;
    if (iters > 0) ctx.core.max_iterations = iters;
    if (palette >= 0) ctx.core.palette_idx = palette % get_palette_count();
    init_color_palette(ctx.core.max_iterations, ctx.core.palette_idx);
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_undo_zoom(void) {
    camera_pop_history(&ctx.core.cam);
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_tour(void) {
    app_state_toggle_tour(&ctx.core, (uint32_t)stm_ms(stm_now()), NULL);
}

EMSCRIPTEN_KEEPALIVE
void wasm_next_palette(void) {
    app_state_cycle_palette(&ctx.core);
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_palette(int idx) {
    if (idx >= 0 && idx < get_palette_count()) {
        ctx.core.palette_idx = idx;
        init_color_palette(ctx.core.max_iterations, ctx.core.palette_idx);
        ctx.core.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia_lock(void) {
    ctx.julia_c_locked = !ctx.julia_c_locked;
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
int wasm_is_julia_locked(void) {
    return ctx.julia_c_locked;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia(void) {
    app_state_toggle_julia(&ctx.core, NULL);
}

EMSCRIPTEN_KEEPALIVE
void wasm_cycle_fractal(void) {
    app_state_cycle_fractal(&ctx.core, NULL);
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_fractal_mode(int mode) {
    if (mode < 0 || mode > 4) mode = 0;
    ctx.core.base_fractal = mode;
    ctx.core.julia_mode = 0;
    ctx.core.julia_session.active = 0;
    camera_reset(&ctx.core.cam);
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_adjust_iterations(int diff) {
    ctx.core.max_iterations += diff;
    if (ctx.core.max_iterations < 10) ctx.core.max_iterations = 10;
    if (ctx.core.max_iterations > get_config_max_iterations_limit())
        ctx.core.max_iterations = get_config_max_iterations_limit();
    init_color_palette(ctx.core.max_iterations, ctx.core.palette_idx);
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_resolution(int w, int h) {
    (void)w;
    (void)h;
}

EMSCRIPTEN_KEEPALIVE
void wasm_cancel_zoom(void) {
    ctx.core.cam.is_panning = 0;
    ctx.core.cam.is_zooming = 0;
    update_zoom_box_js(0, 0, 0, 0, 0);
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_down(float x, float y, int button) {
    AppInputEvent ie = {0};
    ie.type = INPUT_MOUSE_DOWN;
    ie.mouse_btn = (button == 1) ? 3 : 1;
    ie.mouse_x = (int)x;
    ie.mouse_y = (int)y;
    app_handle_input(&ctx.core, &ie, (uint32_t)stm_ms(stm_now()));
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_move(float x, float y) {
    AppInputEvent ie = {0};
    ie.type = INPUT_MOUSE_MOVE;
    ie.mouse_x = (int)x;
    ie.mouse_y = (int)y;
    app_handle_input(&ctx.core, &ie, (uint32_t)stm_ms(stm_now()));
    
    if (ctx.core.cam.is_zooming) {
        int zx = ctx.core.cam.zoom_rect.w > 0 ? ctx.core.cam.zoom_rect.x : ctx.core.cam.zoom_rect.x + ctx.core.cam.zoom_rect.w;
        int zy = ctx.core.cam.zoom_rect.h > 0 ? ctx.core.cam.zoom_rect.y : ctx.core.cam.zoom_rect.y + ctx.core.cam.zoom_rect.h;
        update_zoom_box_js(1, zx, zy, abs(ctx.core.cam.zoom_rect.w), abs(ctx.core.cam.zoom_rect.h));
    } else {
        update_zoom_box_js(0, 0, 0, 0, 0);
    }
    
    if (ctx.core.julia_mode && !ctx.julia_c_locked && ctx.core.m_tour.phase == TOUR_IDLE && !ctx.core.cam.is_panning && !ctx.core.cam.is_zooming) {
        app_state_get_mouse_coord(&ctx.core, ie.mouse_x, ie.mouse_y, &ctx.core.julia_c.re, &ctx.core.julia_c.im);
        ctx.core.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_mouse_up(float x, float y, int button) {
    AppInputEvent ie = {0};
    ie.type = INPUT_MOUSE_UP;
    ie.mouse_btn = (button == 1) ? 3 : 1;
    ie.mouse_x = (int)x;
    ie.mouse_y = (int)y;
    app_handle_input(&ctx.core, &ie, (uint32_t)stm_ms(stm_now()));
    update_zoom_box_js(0, 0, 0, 0, 0);
}

EMSCRIPTEN_KEEPALIVE
void wasm_zoom_at(float cx, float cy, float factor) {
    AppInputEvent ie = {0};
    ie.type = INPUT_MOUSE_SCROLL;
    ie.scroll_y = (factor > 1.0f) ? -1.0f : 1.0f;
    ie.mouse_x = (int)cx;
    ie.mouse_y = (int)cy;
    app_handle_input(&ctx.core, &ie, (uint32_t)stm_ms(stm_now()));
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_gpu(void) {
    ctx.gpu_mode = !ctx.gpu_mode;
    ctx.core.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_request_screenshot(void) {
    ctx.screenshot_requested = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_precision(void) {
    ctx.high_precision_mode = !ctx.high_precision_mode;
    set_cpu_precision(ctx.renderer_ctx, ctx.high_precision_mode);
    ctx.core.needs_redraw = 1;
}

#endif

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    srand((unsigned)time(NULL));
    return (sapp_desc){.init_cb = init,
                       .frame_cb = frame,
                       .cleanup_cb = cleanup,
                       .event_cb = event,
                       .width = 1024,
                       .height = 768,
                       .window_title = "mandelbrot web"};
}
