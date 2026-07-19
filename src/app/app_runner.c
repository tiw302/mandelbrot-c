/* app_runner.c
 *
 * unified high-performance gpu entry point using the sokol framework.
 * manages cpu rendering fallbacks, gpu shaders, deep-zoom perturbation theory,
 * and interactive real-time camera movements.
 */

#if defined(__EMSCRIPTEN__)
#ifndef SOKOL_GLES3
#define SOKOL_GLES3
#endif
#else
#ifndef SOKOL_GLCORE
#define SOKOL_GLCORE
#endif
#endif

#include "app_runner.h"

#include "wasm_bridge.h"

#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL
#define SOKOL_GL_IMPL
#include "app_state.h"
#include "bookmark.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "config_loader.h"
#include "hud_sokol.h"
#include "input_handler.h"
#include "julia.h"
#include "mandelbrot.h"
#include "mandelbrot_bignum.h"
#include "perturbation.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"

// clang-format off
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_gl.h"
#include "shaders.h"
#undef SOKOL_IMPL
#undef Status
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "sokol/sokol_imgui.h"
// clang-format on

// clang-format off
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#elif defined(_WIN32)
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#ifndef GLAPI
#define GLAPI __declspec(dllimport)
#endif
GLAPI void APIENTRY glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void *pixels);
#else
#include <GL/gl.h>
#endif
// clang-format on

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// shaders are automatically embedded from .glsl files at build-time
#define JULIA_ZOOM 4.0
#define MIN_ORBIT_LEN 20

// internal logger callback for sokol diagnostics
static void slog_func(const char* tag, uint32_t log_level, uint32_t log_item_id,
                      const char* message_or_null, uint32_t line_nr, const char* filename_or_null,
                      void* user_data) {
    (void)tag;
    (void)log_level;
    (void)log_item_id;
    (void)line_nr;
    (void)filename_or_null;
    (void)user_data;
    if (message_or_null) printf("[sokol][%d] %s\n", log_level, message_or_null);
}

/*
 * gpu shader uniform block.
 * mirrors the glsl layout exactly to ensure correct memory mapping.
 */

typedef struct {
    // sokol gfx resources
    sg_pipeline pip_cpu, pip_gpu;
    sg_shader shd_cpu, shd_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;

    // cpu rendering mode resources
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t* pixels;  // staging buffer for cpu-mode texture uploads
    int win_w, win_h;

    // persistent buffers for video frame and screenshot capture
    uint32_t* capture_buf;
    int capture_w, capture_h;

    AppCommonState core;

    // modes and telemetry
    AppBackend render_mode;
    int gpu_mode, high_precision_mode;
    int cpu_precision_128;
    int screenshot_requested;
    int quit_pending;
    RendererContext* renderer_ctx;

    // debug ui and text rendering
    sgl_pipeline pip_blend;
    ImFont* custom_font;

    // gpu perturbation specific resources
    sg_image orbit_tex;
    sg_view orbit_tex_view;
    sg_sampler orbit_smp;
    sg_image dummy_img;  // dummy texture bound to slot 0 when perturbation is active
    sg_view dummy_img_view;
    sg_sampler dummy_smp;
    int orbit_len;
    int use_perturbation;
    int active_perturbation_last;  // cached result from orbit computation step
    uint32_t last_interaction_time;
    int was_interacting;
    float ref_offset_x;
    float ref_offset_y;
} GlobalCtx;

static GlobalCtx* g_app_ctx = NULL;

#if defined(__EMSCRIPTEN__)
EMSCRIPTEN_KEEPALIVE
void wasm_toggle_gpu(void) {
    if (g_app_ctx) {
        g_app_ctx->gpu_mode = !g_app_ctx->gpu_mode;
        g_app_ctx->core.needs_redraw = 1;
        app_state_push_notification(&g_app_ctx->core,
                                    g_app_ctx->gpu_mode ? "Engine: GPU" : "Engine: CPU",
                                    stm_ms(stm_now()));
    }
}
EMSCRIPTEN_KEEPALIVE
void wasm_toggle_precision(void) {
    if (g_app_ctx) {
        if (g_app_ctx->gpu_mode) {
            g_app_ctx->high_precision_mode = !g_app_ctx->high_precision_mode;
            app_state_push_notification(&g_app_ctx->core,
                                        g_app_ctx->high_precision_mode
                                            ? "Precision: 64-bit (Double)"
                                            : "Precision: 32-bit (Float)",
                                        stm_ms(stm_now()));
        } else {
#ifdef USE_SIMD_F128
            g_app_ctx->cpu_precision_128 = !g_app_ctx->cpu_precision_128;
            set_cpu_precision(g_app_ctx->renderer_ctx, g_app_ctx->cpu_precision_128);
            app_state_push_notification(&g_app_ctx->core,
                                        g_app_ctx->cpu_precision_128 ? "Precision: 128-bit (SIMD)"
                                                                     : "Precision: 64-bit (Double)",
                                        stm_ms(stm_now()));
#endif
        }
        g_app_ctx->core.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_request_screenshot(void) {
    if (g_app_ctx) {
        g_app_ctx->screenshot_requested = 1;
        g_app_ctx->core.needs_redraw = 1;
        app_state_push_notification(&g_app_ctx->core, "Taking screenshot...", stm_ms(stm_now()));
    }
}
#endif

// re-allocates backend texture and staging buffer on window resize
static void rebuild_texture(GlobalCtx* ctx) {
    if (ctx->img.id) sg_destroy_view(ctx->img_view);
    if (ctx->img.id) sg_destroy_image(ctx->img);
    free(ctx->pixels);
    ctx->pixels = (uint32_t*)malloc((size_t)ctx->win_w * ctx->win_h * 4);
    if (!ctx->pixels) {
        fprintf(stderr, "error: failed to allocate CPU staging buffer\n");
        return;  // can't continue without a pixel buffer
    }
    ctx->img = sg_make_image(&(sg_image_desc){.width = ctx->win_w,
                                              .height = ctx->win_h,
                                              .pixel_format = SG_PIXELFORMAT_RGBA8,
                                              .usage = {.dynamic_update = true}});
    ctx->img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx->img});
    ctx->bind.views[0] = ctx->img_view;
}

// allocates or resizes persistent capture buffers if window size changes
static int ensure_capture_buffers(GlobalCtx* ctx) {
    if (ctx->capture_w != ctx->win_w || ctx->capture_h != ctx->win_h || !ctx->capture_buf) {
        free(ctx->capture_buf);
        ctx->capture_buf = (uint32_t*)malloc((size_t)ctx->win_w * ctx->win_h * sizeof(uint32_t));
        if (!ctx->capture_buf) {
            fprintf(stderr, "error: failed to allocate capture buffers\n");
            free(ctx->capture_buf);
            ctx->capture_buf = NULL;
            ctx->capture_w = 0;
            ctx->capture_h = 0;
            return 0;
        }
        ctx->capture_w = ctx->win_w;
        ctx->capture_h = ctx->win_h;
    }
    return 1;
}

static void init_shaders(GlobalCtx* ctx) {
    if (ctx->pip_cpu.id) sg_destroy_pipeline(ctx->pip_cpu);
    if (ctx->pip_gpu.id) sg_destroy_pipeline(ctx->pip_gpu);
    if (ctx->shd_cpu.id) sg_destroy_shader(ctx->shd_cpu);
    if (ctx->shd_gpu.id) sg_destroy_shader(ctx->shd_gpu);

    ctx->shd_cpu = sg_make_shader(desktop_cpu_shader_desc(sg_query_backend()));
    ctx->shd_gpu = sg_make_shader(desktop_gpu_shader_desc(sg_query_backend()));

    ctx->pip_cpu = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = ctx->shd_cpu,
        .layout = {.attrs = {[ATTR_desktop_cpu_pos] = {.format = SG_VERTEXFORMAT_FLOAT2},
                             [ATTR_desktop_cpu_uv_in] = {.format = SG_VERTEXFORMAT_FLOAT2}}},
        .index_type = SG_INDEXTYPE_UINT16});

    ctx->pip_gpu = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = ctx->shd_gpu,
        .layout = {.attrs = {[ATTR_desktop_gpu_pos] = {.format = SG_VERTEXFORMAT_FLOAT2},
                             [ATTR_desktop_gpu_uv_in] = {.format = SG_VERTEXFORMAT_FLOAT2}}},
        .index_type = SG_INDEXTYPE_UINT16});
}

static void print_controls(void) {
    puts("mandelbrot explorer");
    puts("  left drag   : zoom selection   | right drag  : pan");
    puts("  scroll      : zoom at cursor   | ctrl+z      : undo");
    puts("  up/down     : iterations       | shift+up/dn : x10");
    puts("  p / 0-9     : cycle/set palette| r           : reset");
    puts("  e           : toggle precision | tab         : toggle settings");
    puts("  j / k       : julia mode/lock  |");
    puts("  f / b       : cycle fractals   | s           : screenshot");
    puts("  m           : save bookmark    | l           : load bookmark");
#ifndef BUILD_DEEP_TARGET
    puts("  x           : mega screenshot  | v           : record video");
#endif
    puts("  [ / ]       : scale threads    | h           : toggle help menu");
    puts("  f5          : reload shaders   | q / esc     : quit");
    fflush(stdout);
}

static void init(void* user_data) {
    GlobalCtx* ctx = (GlobalCtx*)user_data;
    if (ctx->render_mode != APP_BACKEND_VIDEO) {
        print_controls();
    }
    sg_setup(&(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
    sgl_setup(&(sgl_desc_t){0});
    stm_setup();

#ifndef __EMSCRIPTEN__
    // cimgui initialization
    simgui_setup(&(simgui_desc_t){.logger.func = slog_func, .no_default_font = true});

    ImGuiIO* io = igGetIO_Nil();
    const char* font_paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                                FONT_PATH_3,     FONT_PATH_4, NULL};
    ctx->custom_font = NULL;
    for (int i = 0; font_paths[i] && font_paths[i][0]; i++) {
        FILE* fp = fopen(font_paths[i], "rb");
        if (fp) {
            fclose(fp);
            ctx->custom_font =
                ImFontAtlas_AddFontFromFileTTF(io->Fonts, font_paths[i], 16.0f, NULL, NULL);
            break;
        }
    }
#endif
    if (!ctx->custom_font) {
        fprintf(stderr, "error: failed to load font\n");
    }

    ctx->win_w = sapp_width();
    ctx->win_h = sapp_height();

    // shared context state
    app_state_init(&ctx->core, ctx->win_w, ctx->win_h);
#if defined(__EMSCRIPTEN__)
    wasm_bridge_init(&ctx->core);
#endif
    ctx->gpu_mode =
        (ctx->render_mode == APP_BACKEND_GPU || ctx->render_mode == APP_BACKEND_WEB) ? 1 : 0;
    ctx->high_precision_mode = 0;
    ctx->cpu_precision_128 = 0;
    ctx->screenshot_requested = 0;

    // full-screen quad for fractal rendering
    float verts[] = {-1, 1, 0, 0, 1, 1, 1, 0, 1, -1, 1, 1, -1, -1, 0, 1};
    ctx->bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});
    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    ctx->bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.index_buffer = true, .immutable = true}, .data = SG_RANGE(idx)});

    ctx->smp = sg_make_sampler(
        &(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR, .mag_filter = SG_FILTER_LINEAR});
    ctx->bind.samplers[0] = ctx->smp;

    ctx->renderer_ctx = init_renderer(ctx->core.max_iterations, ctx->core.palette_idx);
    ctx->core.thread_count = get_actual_thread_count(ctx->renderer_ctx);

    ctx->pip_blend = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0].blend = {.enabled = true,
                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                            .src_factor_alpha = SG_BLENDFACTOR_ONE,
                            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA}});

    // gpu perturbation texture bindings
    ctx->orbit_tex = sg_make_image(&(sg_image_desc){.width = MAX_ITERATIONS_LIMIT,
                                                    .height = 1,
                                                    .pixel_format = SG_PIXELFORMAT_RGBA32F,
                                                    .usage = {.dynamic_update = true}});
    ctx->orbit_tex_view = sg_make_view(&(sg_view_desc){.texture.image = ctx->orbit_tex});
    ctx->orbit_smp = sg_make_sampler(&(sg_sampler_desc){.min_filter = SG_FILTER_NEAREST,
                                                        .mag_filter = SG_FILTER_NEAREST,
                                                        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                                                        .wrap_v = SG_WRAP_CLAMP_TO_EDGE});
    ctx->orbit_len = 0;
    ctx->use_perturbation = 1;
    ctx->active_perturbation_last = 0;
    ctx->last_interaction_time = 0;
    ctx->was_interacting = 0;
    ctx->ref_offset_x = 0.0f;
    ctx->ref_offset_y = 0.0f;

    // sokol dummy texture binding (bound to gpu slot 0 when perturbation is active)
    static uint32_t dummy_pix[1] = {0xFF000000};
    ctx->dummy_img = sg_make_image(&(sg_image_desc){.width = 1,
                                                    .height = 1,
                                                    .pixel_format = SG_PIXELFORMAT_RGBA8,
                                                    .data.mip_levels[0] = SG_RANGE(dummy_pix)});
    ctx->dummy_img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx->dummy_img});
    ctx->dummy_smp = sg_make_sampler(
        &(sg_sampler_desc){.min_filter = SG_FILTER_NEAREST, .mag_filter = SG_FILTER_NEAREST});

    rebuild_texture(ctx);
    init_shaders(ctx);
}

static void frame(void* user_data) {
    GlobalCtx* ctx = (GlobalCtx*)user_data;

    // ensure window dimensions are strictly synced with swapchain (fixes i3wm stretching bugs)
    if (ctx->win_w != sapp_width() || ctx->win_h != sapp_height()) {
        if (sapp_width() > 0 && sapp_height() > 0) {
            ctx->win_w = sapp_width();
            ctx->win_h = sapp_height();
            camera_resize(&ctx->core.cam, ctx->win_w, ctx->win_h);
            rebuild_texture(ctx);
            ctx->core.needs_redraw = 1;
        }
    }

    uint32_t real_now = (uint32_t)stm_ms(stm_now());
    uint32_t now = real_now;

    pthread_mutex_lock(&ctx->core.state_mutex);
    if (ctx->quit_pending && !ctx->core.video_settings.is_rendering) {
        sapp_request_quit();
    }
    pthread_mutex_unlock(&ctx->core.state_mutex);

    // legacy video rendering simulated time logic removed (now handled in background thread)

    // update tour animation state machines
    app_state_update_tours(&ctx->core, now);

    // check for mega screenshot completion
    pthread_mutex_lock(&ctx->core.state_mutex);
    if (ctx->core.mega_screenshot_active == 2 || ctx->core.mega_screenshot_active == 3) {
        char buf[512];
        if (ctx->core.mega_screenshot_active == 2) {
            snprintf(buf, sizeof(buf), "8K Image saved to %s", ctx->core.mega_screenshot_filename);
        } else {
            snprintf(buf, sizeof(buf), "Error: Failed to save 8K Image!");
        }
        app_state_push_notification(&ctx->core, buf, now);
        ctx->core.mega_screenshot_active = 0;
    }
    pthread_mutex_unlock(&ctx->core.state_mutex);

    // check if the user is currently panning or zooming
    int is_interacting = (now - ctx->last_interaction_time < 300);
    if (is_interacting != ctx->was_interacting) {
        ctx->core.needs_redraw = 1;
        ctx->was_interacting = is_interacting;
    }

#if defined(__EMSCRIPTEN__)
    // Performance optimization: skip WebGL rendering if nothing changed
    int should_render =
        ctx->core.needs_redraw || ctx->core.cam.is_panning || ctx->core.cam.is_zooming ||
        ctx->screenshot_requested || (ctx->core.m_tour.phase != TOUR_IDLE) ||
        (ctx->core.j_tour.phase != JULIA_TOUR_IDLE) || (ctx->core.mega_screenshot_active);

    if (ctx->gpu_mode && !should_render) {
        return;  // Retains the canvas (preserveDrawingBuffer) and saves massive battery/CPU
    }
    uint64_t gpu_start_time = stm_now();
#endif

    if (ctx->gpu_mode == 0 && ctx->render_mode != APP_BACKEND_VIDEO) {
        // cpu rendering path (runs the thread pool and updates staging texture)
        if (ctx->core.needs_redraw && ctx->render_mode != APP_BACKEND_VIDEO) {
            precise_float rmin, rmax, imin, imax;
            pthread_mutex_lock(&ctx->core.state_mutex);
            app_state_calculate_boundaries(&ctx->core, ctx->win_w, ctx->win_h, &rmin, &rmax, &imin,
                                           &imax);
            pthread_mutex_unlock(&ctx->core.state_mutex);
            int pitch = ctx->win_w * 4;
            RenderJob job = {.pixels = ctx->pixels,
                             .pitch = pitch,
                             .window_width = ctx->win_w,
                             .window_height = ctx->win_h,
                             .re_min = rmin,
                             .re_max = rmax,
                             .im_top = imax,
                             .im_bottom = imin,
                             .mode = ctx->core.julia_mode ? RENDER_JULIA : ctx->core.base_fractal,
                             .julia_c = ctx->core.julia_c,
                             .max_iterations = ctx->core.max_iterations};
            render_fractal_threaded(ctx->renderer_ctx, &job);
            sg_update_image(
                ctx->img,
                &(sg_image_data){.mip_levels[0] = {.ptr = ctx->pixels,
                                                   .size = (size_t)ctx->win_w * ctx->win_h * 4}});
        }
    } else {
        // gpu rendering path
        int can_use_perturbation =
            ctx->use_perturbation && (ctx->core.cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD) &&
            !ctx->core.julia_mode && (ctx->core.base_fractal == RENDER_MANDELBROT);

        if (!can_use_perturbation) {
            ctx->active_perturbation_last = 0;
        } else if (ctx->core.needs_redraw && ctx->render_mode != APP_BACKEND_VIDEO) {
            precise_float aspect = (precise_float)ctx->win_w / ctx->win_h;
            precise_float zoom = ctx->core.cam.view.zoom;
            precise_float center_re = ctx->core.cam.view.center_re;
            precise_float center_im = ctx->core.cam.view.center_im;
            int max_iters = ctx->core.max_iterations;
            if (max_iters > MAX_ITERATIONS_LIMIT) max_iters = MAX_ITERATIONS_LIMIT;

            int is_interacting = (ctx->core.cam.is_panning || ctx->core.cam.is_zooming);
            int grid_size = is_interacting ? 1 : 11;
            if (is_interacting) {
                if (max_iters > 500) max_iters = 500;
            }
            /* at extreme zoom depths (__float128 precision limit), all grid sample points
             * collapse to the same coordinate. evaluating an 11x11 grid is wasteful and
             * causes a multi-second freeze. force single-point mode below 1e-15. */
            if (zoom < 1e-15) grid_size = 1;
            /* cap orbit iterations at deep zoom to avoid per-frame cpu stalls.
             * at zoom < 1e-7, computing thousands of reference orbit iterations every
             * frame causes visible lag even when not interacting. 800 iterations still
             * gives good visual quality at these depths. */
            // if (zoom < 1e-7 && max_iters > 800) max_iters = 800;

            RefPoint ref =
                find_best_ref_point(center_re, center_im, zoom, aspect, max_iters, grid_size);

            ctx->ref_offset_x = ref.offset_x;
            ctx->ref_offset_y = ref.offset_y;

            /* the camera is capped at 1e-32 (within __float128 range), so the standard
             * double-precision perturbation path handles all reachable zoom levels.
             * the bignum path is disabled: it casts the __float128 coordinate to double
             * before calling perturbation_compute_bignum(), discarding all extra precision
             * and producing a reference orbit that escapes immediately (black screen). */
            RefOrbit* orb = perturbation_compute(ref.ref_re, ref.ref_im, max_iters);

            /* when zoom is deeper than 1e-14, 64-bit emulation underflows and cannot be used as a
             * fallback. thus, we decrease the minimum orbit length threshold to 1 so that the
             * engine stays on the perturbation path even if the reference point escapes early
             * (preventing blocky artifacts). */
            int min_orbit_len = (zoom < 1e-14) ? 1 : MIN_ORBIT_LEN;
            if (orb && orb->len >= min_orbit_len) {
                ctx->orbit_len = orb->len;
                ctx->active_perturbation_last = 1;

                /*
                 * split the 64-bit double-precision reference orbit coordinates into high and low
                 * float parts (dekker split arithmetic). this allows the gpu to reconstruct
                 * double-precision (48-bit mantissa) reference points using standard 32-bit floats,
                 * bypassing opengl's lack of native fp64.
                 */
                typedef struct {
                    float re_hi;
                    float re_lo;
                    float im_hi;
                    float im_lo;
                } OrbitUploadPixel;
                static OrbitUploadPixel orbit_upload_buf[MAX_ITERATIONS_LIMIT];
                for (int k = 0; k < orb->len; k++) {
                    double re = orb->zn[k].re;
                    double im = orb->zn[k].im;
                    orbit_upload_buf[k].re_hi = (float)re;
                    orbit_upload_buf[k].re_lo = (float)(re - (double)orbit_upload_buf[k].re_hi);
                    orbit_upload_buf[k].im_hi = (float)im;
                    orbit_upload_buf[k].im_lo = (float)(im - (double)orbit_upload_buf[k].im_hi);
                }
                if (orb->len < MAX_ITERATIONS_LIMIT) {
                    memset(orbit_upload_buf + orb->len, 0,
                           (MAX_ITERATIONS_LIMIT - orb->len) * sizeof(OrbitUploadPixel));
                }
                sg_update_image(
                    ctx->orbit_tex,
                    &(sg_image_data){.mip_levels[0] = {.ptr = orbit_upload_buf,
                                                       .size = sizeof(orbit_upload_buf)}});
            } else {
                ctx->active_perturbation_last = 0;
            }
            if (orb) perturbation_free(orb);
        }
    }

    sgl_viewport(0, 0, ctx->win_w, ctx->win_h, true);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)ctx->win_w, (float)ctx->win_h, 0.0f, -1.0f, 1.0f);

#ifndef __EMSCRIPTEN__
    simgui_new_frame(&(simgui_frame_desc_t){.width = ctx->win_w,
                                            .height = ctx->win_h,
                                            .delta_time = sapp_frame_duration(),
                                            .dpi_scale = sapp_dpi_scale()});
#endif

    sg_begin_pass(&(sg_pass){.action = ctx->pass_action, .swapchain = sglue_swapchain()});

    if (ctx->gpu_mode) {
        sg_apply_pipeline(ctx->pip_gpu);

        /*
         * bind appropriate texture and sampler:
         * if perturbation is active, we sample from u_orbit texture, and bind dummy_img to slot 0.
         * otherwise, we bind the standard texture.
         */
        int active_perturbation = ctx->active_perturbation_last;

        precise_float aspect = (precise_float)ctx->win_w / ctx->win_h;
        precise_float center_re = ctx->core.cam.view.center_re;
        precise_float center_im = ctx->core.cam.view.center_im;

        params_t_t params = {0};
        params.u_center_hi[0] = (float)center_re;
        params.u_center_lo[0] = (float)(center_re - (precise_float)params.u_center_hi[0]);
        params.u_center_hi[1] = (float)center_im;
        params.u_center_lo[1] = (float)(center_im - (precise_float)params.u_center_hi[1]);
        params.u_julia_c_hi[0] = (float)ctx->core.julia_c.re;
        params.u_julia_c_lo[0] =
            (float)(ctx->core.julia_c.re - (precise_float)params.u_julia_c_hi[0]);
        params.u_julia_c_hi[1] = (float)ctx->core.julia_c.im;
        params.u_julia_c_lo[1] =
            (float)(ctx->core.julia_c.im - (precise_float)params.u_julia_c_hi[1]);
        params.u_zoom = (float)ctx->core.cam.view.zoom;
        params.u_iters = (float)ctx->core.max_iterations;
        params.u_aspect = (float)aspect;
        params.u_fractal_type = (float)(ctx->core.julia_mode ? 1 : ctx->core.base_fractal);
        params.u_palette = (float)ctx->core.palette_idx;

        /* auto-select precision mode based on zoom depth.
         * 32-bit float has ~7 decimal digits of mantissa. at zoom 1e-5, adjacent pixels
         * differ by zoom/width ≈ 5e-9, below float32 epsilon — coordinates collapse and
         * the image becomes a grid of colored blocks.
         * dekker double-single emulates ~14 digits, covering all zoom levels up to 1e-32.
         * perturbation is more accurate when active, so auto-precision is skipped in that case.
         * the manual toggle (key 'e') still overrides this for forcing 32-bit at any zoom. */
        int auto_high_precision = (ctx->core.cam.view.zoom < 1e-5) && !active_perturbation;
        params.u_high_precision = (float)(ctx->high_precision_mode || auto_high_precision);
        params.u_use_perturbation = (float)active_perturbation;
        params.u_orbit_len = (float)ctx->orbit_len;
        params.u_zoom_lo = (float)(ctx->core.cam.view.zoom - (precise_float)params.u_zoom);
        params.u_ref_offset[0] = ctx->ref_offset_x;
        params.u_ref_offset[1] = ctx->ref_offset_y;

        sg_apply_uniforms(UB_params_t, &SG_RANGE(params));
        sg_bindings bind_gpu = ctx->bind;
        bind_gpu.samplers[SMP_u_orbit_smp] = active_perturbation ? ctx->orbit_smp : ctx->dummy_smp;
        bind_gpu.views[VIEW_u_orbit] =
            active_perturbation ? ctx->orbit_tex_view : ctx->dummy_img_view;
        bind_gpu.samplers[SMP_smp] = ctx->smp;
        bind_gpu.views[VIEW_tex] = ctx->img_view;
        sg_apply_bindings(&bind_gpu);
    } else {
        sg_bindings bind_cpu = ctx->bind;
        bind_cpu.samplers[SMP_smp] = ctx->smp;
        bind_cpu.views[VIEW_tex] = ctx->img_view;
        sg_apply_pipeline(ctx->pip_cpu);
        sg_apply_bindings(&bind_cpu);
    }
    // render interactive components using sokol_gl (e.g. selection rectangle)
    if (ctx->core.cam.is_zooming) {
        sgl_load_pipeline(ctx->pip_blend);
        sgl_begin_lines();
        sgl_c4b(255, 255, 0, 255);
        float x0 = (float)ctx->core.cam.zoom_rect.x;
        float y0 = (float)ctx->core.cam.zoom_rect.y;
        float x1 = x0 + (float)ctx->core.cam.zoom_rect.w;
        float y1 = y0 + (float)ctx->core.cam.zoom_rect.h;
        // draw bounds
        sgl_v2f(x0, y0);
        sgl_v2f(x1, y0);
        sgl_v2f(x1, y0);
        sgl_v2f(x1, y1);
        sgl_v2f(x1, y1);
        sgl_v2f(x0, y1);
        sgl_v2f(x0, y1);
        sgl_v2f(x0, y0);
        sgl_end();
    }

    if (ctx->render_mode != APP_BACKEND_VIDEO) {
        sg_draw(0, 6, 1);
        sgl_draw();
    }

#ifndef __EMSCRIPTEN__
    if (ctx->render_mode == APP_BACKEND_VIDEO) {
        int old_prec = ctx->cpu_precision_128;
        hud_render_video_studio(ctx->custom_font, &ctx->core, ctx->win_w, ctx->win_h,
                                &ctx->cpu_precision_128, now);
        if (old_prec != ctx->cpu_precision_128) {
            set_cpu_precision(ctx->renderer_ctx, ctx->cpu_precision_128);
        }
    } else {
        int active_bignum = (ctx->core.cam.view.zoom < BIGNUM_ZOOM_THRESHOLD);
        hud_render_sokol_gpu(ctx->custom_font, &ctx->core, ctx->win_w, ctx->win_h, ctx->gpu_mode,
                             &ctx->high_precision_mode, ctx->cpu_precision_128,
                             ctx->active_perturbation_last, active_bignum, &ctx->use_perturbation,
                             now);
    }

    simgui_render();
#endif

    // capture video frame or screenshot if requested
    if (is_video_recording() || ctx->screenshot_requested) {
        if (ctx->screenshot_requested) {
            if (ensure_capture_buffers(ctx)) {
                glReadPixels(0, 0, ctx->win_w, ctx->win_h, GL_RGBA, GL_UNSIGNED_BYTE,
                             ctx->capture_buf);
                save_screenshot(&ctx->core, ctx->capture_buf, ctx->win_w, ctx->win_h, now, 0, 1);
            }
            ctx->screenshot_requested = 0;
        }
    }

    if (ctx->gpu_mode) {
        ctx->core.needs_redraw = 0;
#if defined(__EMSCRIPTEN__)
        ctx->core.render_time_ms = (uint32_t)stm_ms(stm_diff(stm_now(), gpu_start_time));
        if (ctx->core.render_time_ms == 0)
            ctx->core.render_time_ms = 1;  // Show at least 1ms so it doesn't look broken
#endif
    }

#if defined(__EMSCRIPTEN__)
    call_update_debug_info(
        ctx->gpu_mode, ctx->core.julia_mode, ctx->core.base_fractal, ctx->core.max_iterations,
        ctx->core.cam.view.zoom, ctx->core.cam.view.center_re, ctx->core.cam.view.center_im,
        ctx->core.palette_idx,
        ctx->core.julia_mode ? (int)ctx->core.j_tour.phase : (int)ctx->core.m_tour.phase,
        ctx->core.julia_c.re, ctx->core.julia_c.im, ctx->high_precision_mode,
        ctx->core.julia_mode ? get_julia_tour_target_idx(&ctx->core.j_tour)
                             : get_tour_target_idx(&ctx->core.m_tour),
        ctx->core.julia_mode ? get_num_julia_tour_targets()
                             : get_num_tour_targets(ctx->core.base_fractal),
        ctx->core.julia_mode ? ctx->core.j_tour.to_re
                             : get_tour_target_re(&ctx->core.m_tour, ctx->core.base_fractal),
        ctx->core.julia_mode ? ctx->core.j_tour.to_im
                             : get_tour_target_im(&ctx->core.m_tour, ctx->core.base_fractal),
        ctx->core.thread_count, ctx->core.render_time_ms);
#endif

    sg_end_pass();
    sg_commit();
}

static InputKey map_sokol_key(sapp_keycode sym) {
    switch (sym) {
        case SAPP_KEYCODE_ESCAPE:
            return KEY_ESCAPE;
        case SAPP_KEYCODE_Q:
            return KEY_Q;
        case SAPP_KEYCODE_H:
            return KEY_H;
        case SAPP_KEYCODE_TAB:
            return KEY_TAB;
        case SAPP_KEYCODE_Z:
            return KEY_Z;
        case SAPP_KEYCODE_R:
            return KEY_R;
        case SAPP_KEYCODE_P:
            return KEY_P;
        case SAPP_KEYCODE_0:
            return KEY_0;
        case SAPP_KEYCODE_1:
            return KEY_1;
        case SAPP_KEYCODE_2:
            return KEY_2;
        case SAPP_KEYCODE_3:
            return KEY_3;
        case SAPP_KEYCODE_4:
            return KEY_4;
        case SAPP_KEYCODE_5:
            return KEY_5;
        case SAPP_KEYCODE_6:
            return KEY_6;
        case SAPP_KEYCODE_7:
            return KEY_7;
        case SAPP_KEYCODE_8:
            return KEY_8;
        case SAPP_KEYCODE_9:
            return KEY_9;
        case SAPP_KEYCODE_UP:
            return KEY_UP;
        case SAPP_KEYCODE_DOWN:
            return KEY_DOWN;
        case SAPP_KEYCODE_N:
            return KEY_N;
        case SAPP_KEYCODE_E:
            return KEY_E;
        case SAPP_KEYCODE_J:
            return KEY_J;
        case SAPP_KEYCODE_K:
            return KEY_K;
        case SAPP_KEYCODE_B:
            return KEY_B;
        case SAPP_KEYCODE_F:
            return KEY_F;
        case SAPP_KEYCODE_S:
            return KEY_S;
        case SAPP_KEYCODE_X:
            return KEY_X;
        case SAPP_KEYCODE_V:
            return KEY_V;
        case SAPP_KEYCODE_M:
            return KEY_M;
        case SAPP_KEYCODE_L:
            return KEY_L;
        case SAPP_KEYCODE_T:
            return KEY_T;
        case SAPP_KEYCODE_LEFT_BRACKET:
            return KEY_LEFT_BRACKET;
        case SAPP_KEYCODE_RIGHT_BRACKET:
            return KEY_RIGHT_BRACKET;
        case SAPP_KEYCODE_F5:
            return KEY_F5;
        default:
            return KEY_UNKNOWN;
    }
}

// handles window events and input mapping
static void event(const sapp_event* ev, void* user_data) {
    GlobalCtx* ctx = (GlobalCtx*)user_data;
#ifndef __EMSCRIPTEN__
    if (simgui_handle_event(ev)) {
        return;
    }
#endif
    uint32_t now = (uint32_t)stm_ms(stm_now());
    AppInputEvent ie = {0};
    int handled = 0;

    switch (ev->type) {
        case SAPP_EVENTTYPE_QUIT_REQUESTED:
            if (ctx->core.video_settings.is_rendering || is_video_recording()) {
                sapp_cancel_quit();
                ctx->core.video_settings.export_cancelled = 1;
                app_state_push_notification(&ctx->core, "Safely shutting down video export...",
                                            now);
            }
            handled = 1;
            break;

        case SAPP_EVENTTYPE_RESIZED:
            if (ev->window_width > 0 && ev->window_height > 0) {
                ctx->win_w = ev->framebuffer_width;
                ctx->win_h = ev->framebuffer_height;
                camera_resize(&ctx->core.cam, ctx->win_w, ctx->win_h);
                rebuild_texture(ctx);
                ctx->core.needs_redraw = 1;
            }
            handled = 1;
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
#ifdef __EMSCRIPTEN__
            if (!ev->key_repeat) {
                if (ev->key_code == SAPP_KEYCODE_I) {
                    EM_ASM({ if (typeof toggleInfo === 'function') toggleInfo(); });
                } else if (ev->key_code == SAPP_KEYCODE_O) {
                    EM_ASM({ if (typeof toggleSettings === 'function') toggleSettings(); });
                } else if (ev->key_code == SAPP_KEYCODE_C) {
                    EM_ASM({ if (typeof copyLink === 'function') copyLink(); });
                } else if (ev->key_code == SAPP_KEYCODE_L) {
                    EM_ASM({ if (typeof toggleJuliaLock === 'function') toggleJuliaLock(); });
                }
            }
#endif
            ie.type = INPUT_KEY_DOWN;
            ie.key = map_sokol_key(ev->key_code);
#ifdef __EMSCRIPTEN__
            if (ev->key_code == SAPP_KEYCODE_I || ev->key_code == SAPP_KEYCODE_O || 
                ev->key_code == SAPP_KEYCODE_C || ev->key_code == SAPP_KEYCODE_L) {
                ie.key = KEY_UNKNOWN;
            }
#endif
            ie.mod_shift = (ev->modifiers & SAPP_MODIFIER_SHIFT) ? 1 : 0;
            ie.mod_ctrl = (ev->modifiers & SAPP_MODIFIER_CTRL) ? 1 : 0;
            break;

        case SAPP_EVENTTYPE_KEY_UP:
            ie.type = INPUT_KEY_UP;
            ie.key = map_sokol_key(ev->key_code);
#ifdef __EMSCRIPTEN__
            if (ev->key_code == SAPP_KEYCODE_I || ev->key_code == SAPP_KEYCODE_O || 
                ev->key_code == SAPP_KEYCODE_C || ev->key_code == SAPP_KEYCODE_L) {
                ie.key = KEY_UNKNOWN;
            }
#endif
            ie.mod_shift = (ev->modifiers & SAPP_MODIFIER_SHIFT) ? 1 : 0;
            ie.mod_ctrl = (ev->modifiers & SAPP_MODIFIER_CTRL) ? 1 : 0;
            break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            ie.type = INPUT_MOUSE_SCROLL;
            ie.scroll_y = ev->scroll_y;
            ctx->last_interaction_time = now;
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN:
            ie.type = INPUT_MOUSE_DOWN;
            ie.mouse_btn = (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            ctx->last_interaction_time = now;
            break;

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            ie.type = INPUT_MOUSE_MOVE;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            if (ctx->core.cam.is_panning) {
                ctx->last_interaction_time = now;
            }
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
        pthread_mutex_lock(&ctx->core.state_mutex);
        InputAction action = app_handle_input(&ctx->core, &ie, now);
        pthread_mutex_unlock(&ctx->core.state_mutex);
        switch (action) {
            case ACTION_QUIT:
                sapp_request_quit();
                break;
            case ACTION_TOGGLE_PERTURBATION:
                ctx->use_perturbation = !ctx->use_perturbation;
                ctx->core.needs_redraw = 1;
                app_state_push_notification(
                    &ctx->core,
                    ctx->use_perturbation ? "Perturbation: Enabled" : "Perturbation: Disabled",
                    now);
                break;
            case ACTION_TOGGLE_GPU:
                ctx->gpu_mode = !ctx->gpu_mode;
                ctx->core.needs_redraw = 1;
                app_state_push_notification(&ctx->core,
                                            ctx->gpu_mode ? "Engine: GPU" : "Engine: CPU", now);
                break;
            case ACTION_TOGGLE_PRECISION:
                if (ctx->gpu_mode) {
                    ctx->high_precision_mode = !ctx->high_precision_mode;
                    app_state_push_notification(&ctx->core,
                                                ctx->high_precision_mode
                                                    ? "Precision: 64-bit (Double)"
                                                    : "Precision: 32-bit (Float)",
                                                now);
                } else {
#ifdef USE_SIMD_F128
                    ctx->cpu_precision_128 = !ctx->cpu_precision_128;
                    set_cpu_precision(ctx->renderer_ctx, ctx->cpu_precision_128);
                    app_state_push_notification(&ctx->core,
                                                ctx->cpu_precision_128
                                                    ? "Precision: 128-bit (SIMD)"
                                                    : "Precision: 64-bit (Double)",
                                                now);
#endif
                }
                ctx->core.needs_redraw = 1;
                break;

            case ACTION_MEGA_SCREENSHOT:
                if (!ctx->gpu_mode && ctx->core.mega_screenshot_active == 0) {
                    precise_float rmin, rmax, imin, imax;
                    app_state_calculate_boundaries(&ctx->core, ctx->win_w, ctx->win_h, &rmin, &rmax,
                                                   &imin, &imax);
                    save_mega_screenshot_async(
                        ctx->renderer_ctx, &ctx->core, 8192, 8192, rmin, rmax, imin, imax,
                        ctx->core.max_iterations,
                        ctx->core.julia_mode ? RENDER_JULIA : ctx->core.base_fractal,
                        ctx->core.julia_c);
                    ctx->core.needs_redraw = 1;
                    app_state_push_notification(&ctx->core, "Generating 8K Image...", now);
                }
                break;
            case ACTION_TOGGLE_VIDEO:
                if (ctx->core.video_settings.is_rendering) {
                    ctx->core.video_settings.export_cancelled = 1;
                    app_state_push_notification(&ctx->core, "cancelling video export...", now);
                } else {
                    start_video_export_async(&ctx->core);
                    app_state_push_notification(&ctx->core, "video export started", now);
                }
                break;
            case ACTION_RESIZE_THREADS_UP:
            case ACTION_RESIZE_THREADS_DOWN: {
                if (ctx->core.mega_screenshot_active == 1) {
                    app_state_push_notification(&ctx->core, "screenshot in progress...", now);
                    break;
                }
                int threads = get_actual_thread_count(ctx->renderer_ctx);
                threads += (action == ACTION_RESIZE_THREADS_UP) ? 1 : -1;
                set_renderer_thread_count(ctx->renderer_ctx, threads);
                ctx->core.thread_count = get_actual_thread_count(ctx->renderer_ctx);
                ctx->core.needs_redraw = 1;
                char buf[64];
                snprintf(buf, sizeof(buf), "CPU Threads: %d Cores", ctx->core.thread_count);
                app_state_push_notification(&ctx->core, buf, now);
                break;
            }
            case ACTION_RELOAD_SHADERS:
                init_shaders(ctx);
                ctx->core.needs_redraw = 1;
                app_state_push_notification(&ctx->core, "shaders reloaded", now);
                break;
            default:
                if (ie.type == INPUT_KEY_DOWN && ie.key == KEY_S) {
                    ctx->screenshot_requested = 1;
                    ctx->core.needs_redraw = 1;
                }
                break;
        }
    }
}

static void cleanup(void* user_data) {
    GlobalCtx* ctx = (GlobalCtx*)user_data;
    bookmark_cache_free();
    free(ctx->pixels);
    free(ctx->capture_buf);
    cleanup_renderer(ctx->renderer_ctx);
    cleanup_color_palette();
#ifndef __EMSCRIPTEN__
    simgui_shutdown();
#endif
    if (ctx->shd_cpu.id) sg_destroy_shader(ctx->shd_cpu);
    if (ctx->shd_gpu.id) sg_destroy_shader(ctx->shd_gpu);
    if (ctx->orbit_tex.id) sg_destroy_view(ctx->orbit_tex_view);
    if (ctx->orbit_tex.id) sg_destroy_image(ctx->orbit_tex);
    if (ctx->orbit_smp.id) sg_destroy_sampler(ctx->orbit_smp);
    if (ctx->dummy_img.id) sg_destroy_view(ctx->dummy_img_view);
    if (ctx->dummy_img.id) sg_destroy_image(ctx->dummy_img);
    if (ctx->dummy_smp.id) sg_destroy_sampler(ctx->dummy_smp);
    sgl_shutdown();
    sg_shutdown();
    pthread_mutex_destroy(&ctx->core.state_mutex);
    free(ctx);
}

sapp_desc app_runner_get_desc(AppBackend mode) {
    GlobalCtx* ctx = (GlobalCtx*)calloc(1, sizeof(GlobalCtx));
    g_app_ctx = ctx;
    ctx->render_mode = mode;
    load_config_from_file("settings.json");
    return (sapp_desc){.init_userdata_cb = init,
                       .frame_userdata_cb = frame,
                       .cleanup_userdata_cb = cleanup,
                       .event_userdata_cb = event,
                       .user_data = ctx,
                       .width = mode == APP_BACKEND_VIDEO ? 1600 : 1280,
                       .height = mode == APP_BACKEND_VIDEO ? 900 : 720,
                       .window_title = mode == APP_BACKEND_VIDEO ? "Mandelbrot Video Studio"
                                                                 : "Mandelbrot Explorer",
                       .logger.func = slog_func,
                       .icon.sokol_default = true};
}
