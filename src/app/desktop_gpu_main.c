/* desktop_gpu_main.c
 *
 * unified high-performance gpu entry point using the sokol framework.
 * manages cpu rendering fallbacks, gpu shaders, deep-zoom perturbation theory,
 * and interactive real-time camera movements.
 */

#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL
#define SOKOL_GL_IMPL
#include "app_state.h"
#include "bookmark.h"
#include "tour.h"
#include "input_handler.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "hud_sokol.h"
#include "ini_config.h"
#include "julia.h"
#include "mandelbrot.h"
#include "renderer.h"
#include "screenshot.h"
#include "perturbation.h"
#include "settings_panel.h"

// clang-format off
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_gl.h"
#define FONTSTASH_IMPLEMENTATION
#define SOKOL_FONTSTASH_IMPL
#include "fons/fontstash.h"
#include "sokol/sokol_fontstash.h"
#undef SOKOL_IMPL
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

// Shaders are automatically embedded from .glsl files at build-time
#include "desktop_gpu_vs.h"
#include "desktop_gpu_fs_cpu.h"
#include "desktop_gpu_fs_gpu.h"

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

// GPU shader uniform block.
// Mirrors the GLSL layout exactly to ensure correct memory mapping.
typedef struct {
    float center_hi[2];
    float center_lo[2];
    float julia_c_hi[2];
    float julia_c_lo[2];
    float zoom, iters, aspect;
    float fractal_type, palette, high_precision;
    float use_perturbation;
    float orbit_len;
    float zoom_lo;  // low part of zoom for Dekker hi-lo in perturbation d0 calc
    float ref_offset[2];
} params_t;

// application global context
typedef struct {
    // sokol gfx resources
    sg_pipeline pip_cpu, pip_gpu;
    sg_shader shd_cpu, shd_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    
    // CPU rendering mode resources
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t* pixels;  // staging buffer for cpu-mode texture uploads
    int win_w, win_h;

    // persistent buffers for video frame and screenshot capture
    uint32_t* capture_buf;
    uint32_t* flipped_buf;
    int capture_w, capture_h;

    // common application state
    AppCommonState core;

    // modes and telemetry
    int gpu_mode, high_precision_mode;
    int cpu_precision_128;
    int screenshot_requested;
    RendererContext* renderer_ctx;

    // debug ui and text rendering
    sgl_pipeline pip_blend;
    FONScontext* fons;
    int font_id;

    // GPU perturbation specific resources
    sg_image orbit_tex;
    sg_view orbit_tex_view;
    sg_sampler orbit_smp;
    sg_image dummy_img; // dummy texture bound to slot 0 when perturbation is active
    sg_view dummy_img_view;
    sg_sampler dummy_smp;
    int orbit_len;
    int use_perturbation;
    int active_perturbation_last; // cached result from orbit computation step
    uint32_t last_interaction_time;
    int was_interacting;
    float ref_offset_x;
    float ref_offset_y;

    // settings panel
    SettingsPanel settings;
} GlobalCtx;

static GlobalCtx ctx;

// callback to update the sokol window title
static void set_window_title_cb(const char* title) {
    sapp_set_window_title(title);
}

// re-allocates backend texture and staging buffer on window resize
static void rebuild_texture(void) {
    if (ctx.img.id) sg_destroy_view(ctx.img_view);
    if (ctx.img.id) sg_destroy_image(ctx.img);
    free(ctx.pixels);
    ctx.pixels = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * 4);
    if (!ctx.pixels) {
        fprintf(stderr, "error: failed to allocate CPU staging buffer\n");
    }
    ctx.img = sg_make_image(&(sg_image_desc){.width = ctx.win_w,
                                             .height = ctx.win_h,
                                             .pixel_format = SG_PIXELFORMAT_RGBA8,
                                             .usage = {.dynamic_update = true}});
    ctx.img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.img});
    ctx.bind.views[0] = ctx.img_view;
}

// allocates or resizes persistent capture buffers if window size changes
static int ensure_capture_buffers(void) {
    if (ctx.capture_w != ctx.win_w || ctx.capture_h != ctx.win_h || !ctx.capture_buf ||
        !ctx.flipped_buf) {
        free(ctx.capture_buf);
        free(ctx.flipped_buf);
        ctx.capture_buf = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * sizeof(uint32_t));
        ctx.flipped_buf = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * sizeof(uint32_t));
        if (!ctx.capture_buf || !ctx.flipped_buf) {
            fprintf(stderr, "error: failed to allocate capture buffers\n");
            free(ctx.capture_buf);
            free(ctx.flipped_buf);
            ctx.capture_buf = NULL;
            ctx.flipped_buf = NULL;
            ctx.capture_w = 0;
            ctx.capture_h = 0;
            return 0;
        }
        ctx.capture_w = ctx.win_w;
        ctx.capture_h = ctx.win_h;
    }
    return 1;
}

static char* read_shader_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

static void reload_shaders(void) {
    // try to load shaders from files; fallback to embedded variables if not found
    char* vs_src = read_shader_file("shaders/desktop_gpu_vs.glsl");
    char* fs_cpu_src = read_shader_file("shaders/desktop_gpu_fs_cpu.glsl");
    char* fs_gpu_src = read_shader_file("shaders/desktop_gpu_fs_gpu.glsl");
    const char* vs_ptr = vs_src ? vs_src : dg_vs;
    const char* fs_cpu_ptr = fs_cpu_src ? fs_cpu_src : dg_fs_cpu;
    const char* fs_gpu_ptr = fs_gpu_src ? fs_gpu_src : dg_fs_gpu;

    // compile shader programs
    sg_shader shd_cpu = sg_make_shader(
        &(sg_shader_desc){.attrs[0].glsl_name = "pos",
                          .attrs[1].glsl_name = "uv_in",
                          .vertex_func.source = vs_ptr,
                          .fragment_func.source = fs_cpu_ptr,
                          .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
                          .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
                          .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                                       .view_slot = 0,
                                                       .sampler_slot = 0,
                                                       .glsl_name = "tex"}});

    sg_shader shd_gpu = sg_make_shader(&(sg_shader_desc){
        .attrs[0].glsl_name = "pos",
        .attrs[1].glsl_name = "uv_in",
        .vertex_func.source = vs_ptr,
        .fragment_func.source = fs_gpu_ptr,
        .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                     .view_slot = 0,
                                     .sampler_slot = 0,
                                     .glsl_name = "u_orbit"},
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(params_t),
            .glsl_uniforms = {
                {.glsl_name = "u_center_hi", .type = SG_UNIFORMTYPE_FLOAT2},
                {.glsl_name = "u_center_lo", .type = SG_UNIFORMTYPE_FLOAT2},
                {.glsl_name = "u_julia_c_hi", .type = SG_UNIFORMTYPE_FLOAT2},
                {.glsl_name = "u_julia_c_lo", .type = SG_UNIFORMTYPE_FLOAT2},
                {.glsl_name = "u_zoom", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_iters", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_aspect", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_fractal_type", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_palette", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_high_precision", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_use_perturbation", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_orbit_len", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_zoom_lo", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_ref_offset", .type = SG_UNIFORMTYPE_FLOAT2},
            }}});

    if (sg_query_shader_state(shd_cpu) != SG_RESOURCESTATE_VALID ||
        sg_query_shader_state(shd_gpu) != SG_RESOURCESTATE_VALID) {
        printf("error: failed to compile shaders from files. keeping existing shaders.\n");
        sg_destroy_shader(shd_cpu);
        sg_destroy_shader(shd_gpu);
        free(vs_src);
        free(fs_cpu_src);
        free(fs_gpu_src);
        return;
    }

    // safely destroy previous assets to prevent leaks
    if (ctx.pip_cpu.id) sg_destroy_pipeline(ctx.pip_cpu);
    if (ctx.pip_gpu.id) sg_destroy_pipeline(ctx.pip_gpu);
    if (ctx.shd_cpu.id) sg_destroy_shader(ctx.shd_cpu);
    if (ctx.shd_gpu.id) sg_destroy_shader(ctx.shd_gpu);

    ctx.shd_cpu = shd_cpu;
    ctx.shd_gpu = shd_gpu;

    // build Sokol pipelines matching our layout
    ctx.pip_cpu = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = ctx.shd_cpu,
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
        .index_type = SG_INDEXTYPE_UINT16
    });

    ctx.pip_gpu = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = ctx.shd_gpu,
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
        .index_type = SG_INDEXTYPE_UINT16
    });

    free(vs_src);
    free(fs_cpu_src);
    free(fs_gpu_src);
    printf("shaders loaded/reloaded successfully.\n");
}

static void init(void) {
    sg_setup(&(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
    sgl_setup(&(sgl_desc_t){0});
    stm_setup();

    // fontstash initialization
    ctx.fons = sfons_create(&(sfons_desc_t){.width = 512, .height = 512});
    const char* font_paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                                FONT_PATH_3,     FONT_PATH_4, NULL};
    ctx.font_id = FONS_INVALID;
    for (int i = 0; font_paths[i] && font_paths[i][0]; i++) {
        ctx.font_id = fonsAddFont(ctx.fons, "sans", font_paths[i]);
        if (ctx.font_id != FONS_INVALID) break;
    }
    if (ctx.font_id == FONS_INVALID) {
        fprintf(stderr, "error: failed to load font\n");
    }

    ctx.win_w = sapp_width();
    ctx.win_h = sapp_height();

    // shared context state
    app_state_init(&ctx.core, ctx.win_w, ctx.win_h);
    ctx.gpu_mode = 1;
    ctx.high_precision_mode = 1;
    ctx.cpu_precision_128 = 0;
    ctx.screenshot_requested = 0;

    // full-screen quad for fractal rendering
    float verts[] = {-1, 1, 0, 0, 1, 1, 1, 0, 1, -1, 1, 1, -1, -1, 0, 1};
    ctx.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});
    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    ctx.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.index_buffer = true, .immutable = true}, .data = SG_RANGE(idx)});

    ctx.smp = sg_make_sampler(
        &(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR, .mag_filter = SG_FILTER_LINEAR});
    ctx.bind.samplers[0] = ctx.smp;

    // CPU thread pool initialization
    ctx.renderer_ctx = init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
    ctx.core.thread_count = get_actual_thread_count(ctx.renderer_ctx);

    // Sokol GL text blending pipeline
    ctx.pip_blend = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_ONE,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
        }
    });

    // GPU perturbation texture bindings
    ctx.orbit_tex = sg_make_image(&(sg_image_desc){
        .width = MAX_ITERATIONS_LIMIT,
        .height = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA32F,
        .usage = {.dynamic_update = true}
    });
    ctx.orbit_tex_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.orbit_tex});
    ctx.orbit_smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    ctx.orbit_len = 0;
    ctx.use_perturbation = 1;
    ctx.active_perturbation_last = 0;
    ctx.last_interaction_time = 0;
    ctx.was_interacting = 0;
    ctx.ref_offset_x = 0.0f;
    ctx.ref_offset_y = 0.0f;

    // Sokol dummy texture binding (bound to GPU slot 0 when perturbation is active)
    static uint32_t dummy_pix[1] = {0xFF000000};
    ctx.dummy_img = sg_make_image(&(sg_image_desc){
        .width = 1,
        .height = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.mip_levels[0] = SG_RANGE(dummy_pix)
    });
    ctx.dummy_img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.dummy_img});
    ctx.dummy_smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST, .mag_filter = SG_FILTER_NEAREST
    });

    rebuild_texture();
    reload_shaders();
}

static void frame(void) {
    uint32_t now = (uint32_t)sapp_frame_duration() * 1000;
    now = (uint32_t)stm_ms(stm_now());
    
    // update tour animation state machines
    app_state_update_tours(&ctx.core, now, set_window_title_cb);

    // check if the user is currently panning or zooming
    int is_interacting = (now - ctx.last_interaction_time < 300);
    if (is_interacting != ctx.was_interacting) {
        ctx.core.needs_redraw = 1;
        ctx.was_interacting = is_interacting;
    }

    if (ctx.gpu_mode == 0) {
        // CPU rendering path (runs the thread pool and updates staging texture)
        if (ctx.core.needs_redraw) {
            precise_float rmin, rmax, imin, imax;
            app_state_calculate_boundaries(&ctx.core, ctx.win_w, ctx.win_h, &rmin, &rmax, &imin, &imax);
            int pitch = ctx.win_w * 4;
            if (ctx.core.julia_mode) {
                render_julia_threaded(ctx.renderer_ctx, ctx.pixels, pitch, ctx.win_w, ctx.win_h, rmin, rmax, imax, imin,
                                      ctx.core.julia_c, ctx.core.max_iterations);
            } else if (ctx.core.base_fractal == RENDER_BURNING_SHIP) {
                render_burning_ship_threaded(ctx.renderer_ctx, ctx.pixels, pitch, ctx.win_w, ctx.win_h, rmin, rmax,
                                             imax, imin, ctx.core.max_iterations);
            } else {
                render_mandelbrot_threaded(ctx.renderer_ctx, ctx.pixels, pitch, ctx.win_w, ctx.win_h, rmin, rmax, imax,
                                           imin, ctx.core.max_iterations);
            }
            sg_update_image(ctx.img, &(sg_image_data){.mip_levels[0] = {.ptr = ctx.pixels, .size = (size_t)ctx.win_w * ctx.win_h * 4}});
        }
    } else {
        // GPU rendering path
        int can_use_perturbation = ctx.use_perturbation &&
                                   (ctx.core.cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD);

        if (!can_use_perturbation) {
            ctx.active_perturbation_last = 0;
        } else if (ctx.core.needs_redraw) {
            precise_float aspect = (precise_float)ctx.win_w / ctx.win_h;
            precise_float zoom = ctx.core.cam.view.zoom;
            precise_float center_re = ctx.core.cam.view.center_re;
            precise_float center_im = ctx.core.cam.view.center_im;
            int max_iters = ctx.core.max_iterations;
            if (max_iters > MAX_ITERATIONS_LIMIT) max_iters = MAX_ITERATIONS_LIMIT;

            // Grid Search to find a reference coordinate with the maximum escape iterations.
            // We use an 11x11 grid (121 points) at all times (including during mouse interaction)
            // to ensure we land on thin filaments and prevent early escapes.
            precise_float best_re = center_re;
            precise_float best_im = center_im;
            float best_gx = 0.0f;
            float best_gy = 0.0f;
            double best_score = -1.0;

            int grid_size = 11;
            for (int gy = 0; gy < grid_size; gy++) {
                float ny = (grid_size > 1) ? ((float)gy / (grid_size - 1) - 0.5f) : 0.0f;
                for (int gx = 0; gx < grid_size; gx++) {
                    float nx = (grid_size > 1) ? ((float)gx / (grid_size - 1) - 0.5f) : 0.0f;
                    precise_float c_re = center_re + (precise_float)nx * zoom * aspect;
                    precise_float c_im = center_im + (precise_float)ny * zoom;

                    // compute escape iterations for this candidate point on the CPU
                    int iters = 0;
                    precise_float z_re = 0.0;
                    precise_float z_im = 0.0;
                    const precise_float escape_radius_sq = ESCAPE_RADIUS * ESCAPE_RADIUS;
                    for (; iters < max_iters; iters++) {
                        precise_float z_re2 = z_re * z_re;
                        precise_float z_im2 = z_im * z_im;
                        if (z_re2 + z_im2 > escape_radius_sq) {
                            break;
                        }
                        z_im = 2.0 * z_re * z_im + c_im;
                        z_re = z_re2 - z_im2 + c_re;
                    }

                    // calculate score: favor points that escape later, breaking ties with closeness to center
                    double dist_sq = (double)(nx * nx + ny * ny);
                    double score = (double)iters - 1e-5 * dist_sq;
                    if (score > best_score) {
                        best_score = score;
                        best_re = c_re;
                        best_im = c_im;
                        best_gx = nx;
                        best_gy = ny;
                    }
                }
            }

            ctx.ref_offset_x = best_gx;
            ctx.ref_offset_y = best_gy;

            // Compute the reference orbit starting at the chosen best coordinate
            RefOrbit* orb = perturbation_compute(best_re, best_im, max_iters);

            // When zoom is deeper than 1e-14, 64-bit emulation underflows and cannot be used as a fallback.
            // Thus, we decrease the minimum orbit length threshold to 1 so that the engine stays on the
            // perturbation path even if the reference point escapes early (preventing blocky artifacts).
            int min_orbit_len = (zoom < 1e-14) ? 1 : MIN_ORBIT_LEN;
            if (orb && orb->len >= min_orbit_len) {
                ctx.orbit_len = orb->len;
                ctx.active_perturbation_last = 1;

                // Split the 64-bit double-precision reference orbit coordinates into high and low float parts
                // (Dekker split arithmetic). This allows the GPU to reconstruct double-precision (48-bit mantissa)
                // reference points using standard 32-bit floats, bypassing OpenGL's lack of native fp64.
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
                sg_update_image(ctx.orbit_tex, &(sg_image_data){
                    .mip_levels[0] = {.ptr = orbit_upload_buf,
                                      .size = sizeof(orbit_upload_buf)}});
            } else {
                ctx.active_perturbation_last = 0;
            }
            if (orb) perturbation_free(orb);
        }
    }

    // background orchestration (drawing the fractal)
    sgl_viewport(0, 0, ctx.win_w, ctx.win_h, true);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)ctx.win_w, (float)ctx.win_h, 0.0f, -1.0f, 1.0f);

    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});

    if (ctx.gpu_mode) {
        sg_apply_pipeline(ctx.pip_gpu);

        // Bind appropriate texture and sampler:
        // If perturbation is active, we sample from u_orbit texture, and bind dummy_img to slot 0.
        // Otherwise, we bind the standard texture.
        int active_perturbation = ctx.active_perturbation_last;
        if (active_perturbation) {
            ctx.bind.views[0] = ctx.orbit_tex_view;
            ctx.bind.samplers[0] = ctx.orbit_smp;
        } else {
            ctx.bind.views[0] = ctx.img_view;
            ctx.bind.samplers[0] = ctx.smp;
        }

        precise_float aspect = (precise_float)ctx.win_w / ctx.win_h;
        precise_float center_re = ctx.core.cam.view.center_re;
        precise_float center_im = ctx.core.cam.view.center_im;

        params_t params = {0};
        params.center_hi[0] = (float)center_re;
        params.center_lo[0] = (float)(center_re - (precise_float)params.center_hi[0]);
        params.center_hi[1] = (float)center_im;
        params.center_lo[1] = (float)(center_im - (precise_float)params.center_hi[1]);
        params.julia_c_hi[0] = (float)ctx.core.julia_c.re;
        params.julia_c_lo[0] = (float)(ctx.core.julia_c.re - (precise_float)params.julia_c_hi[0]);
        params.julia_c_hi[1] = (float)ctx.core.julia_c.im;
        params.julia_c_lo[1] = (float)(ctx.core.julia_c.im - (precise_float)params.julia_c_hi[1]);
        params.zoom = (float)ctx.core.cam.view.zoom;
        params.iters = (float)ctx.core.max_iterations;
        params.aspect = (float)aspect;
        params.fractal_type = (float)(ctx.core.julia_mode ? 1 : ctx.core.base_fractal);
        params.palette = (float)ctx.core.palette_idx;
        params.high_precision = (float)ctx.high_precision_mode;
        params.use_perturbation = (float)active_perturbation;
        params.orbit_len = (float)ctx.orbit_len;
        params.zoom_lo = (float)(ctx.core.cam.view.zoom - (precise_float)params.zoom);
        params.ref_offset[0] = ctx.ref_offset_x;
        params.ref_offset[1] = ctx.ref_offset_y;

        sg_apply_uniforms(0, &SG_RANGE(params));
    } else {
        ctx.bind.views[0] = ctx.img_view;
        ctx.bind.samplers[0] = ctx.smp;
        sg_apply_pipeline(ctx.pip_cpu);
    }
    sg_apply_bindings(&ctx.bind);
    sg_draw(0, 6, 1);

    // render interactive components using sokol_gl (e.g. selection rectangle)
    if (ctx.core.cam.is_zooming) {
        sgl_load_pipeline(ctx.pip_blend);
        sgl_begin_lines();
        sgl_c4b(255, 255, 0, 255);
        float x0 = (float)ctx.core.cam.zoom_rect.x;
        float y0 = (float)ctx.core.cam.zoom_rect.y;
        float x1 = x0 + (float)ctx.core.cam.zoom_rect.w;
        float y1 = y0 + (float)ctx.core.cam.zoom_rect.h;
        // draw bounds
        sgl_v2f(x0, y0); sgl_v2f(x1, y0);
        sgl_v2f(x1, y0); sgl_v2f(x1, y1);
        sgl_v2f(x1, y1); sgl_v2f(x0, y1);
        sgl_v2f(x0, y1); sgl_v2f(x0, y0);
        sgl_end();
    }

    // render HUD (telemetry overlay)
    hud_render_sokol_gpu(ctx.fons, ctx.font_id, &ctx.core, ctx.win_w, ctx.win_h, ctx.gpu_mode,
                         ctx.high_precision_mode, ctx.cpu_precision_128,
                         ctx.active_perturbation_last, ctx.use_perturbation,
                         ctx.pip_blend, now);

    // render settings panel (if open)
    settings_panel_render(&ctx.settings, ctx.fons, ctx.font_id, &ctx.core,
                          ctx.win_w, ctx.win_h, ctx.pip_blend, now);

    sgl_draw();

    // capture video frame or screenshot if requested
    if (is_video_recording() || ctx.screenshot_requested) {
        if (ensure_capture_buffers()) {
            glReadPixels(0, 0, ctx.win_w, ctx.win_h, GL_RGBA, GL_UNSIGNED_BYTE, ctx.capture_buf);
            if (is_video_recording()) {
                append_video_frame(ctx.capture_buf, ctx.win_w, ctx.win_h);
            }
            if (ctx.screenshot_requested) {
                save_screenshot(&ctx.core, ctx.capture_buf, ctx.win_w, ctx.win_h, now, 0, 1);
                ctx.screenshot_requested = 0;
            }
        } else {
            ctx.screenshot_requested = 0;
        }
    }

    if (ctx.gpu_mode) {
        ctx.core.needs_redraw = 0;
    }

    sg_end_pass();
    sg_commit();
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
        case SAPP_KEYCODE_J: return KEY_J;
        case SAPP_KEYCODE_B: return KEY_B;
        case SAPP_KEYCODE_F: return KEY_F;
        case SAPP_KEYCODE_S: return KEY_S;
        case SAPP_KEYCODE_X: return KEY_X;
        case SAPP_KEYCODE_V: return KEY_V;
        case SAPP_KEYCODE_M: return KEY_M;
        case SAPP_KEYCODE_L: return KEY_L;
        case SAPP_KEYCODE_T: return KEY_T;
        case SAPP_KEYCODE_LEFT_BRACKET: return KEY_LEFT_BRACKET;
        case SAPP_KEYCODE_RIGHT_BRACKET: return KEY_RIGHT_BRACKET;
        case SAPP_KEYCODE_F5: return KEY_F5;
        case SAPP_KEYCODE_I: return KEY_I;
        default: return KEY_UNKNOWN;
    }
}

// handles window events and input mapping
static void event(const sapp_event* ev) {
    uint32_t now = (uint32_t)stm_ms(stm_now());
    AppInputEvent ie = {0};
    int handled = 0;

    switch (ev->type) {
        case SAPP_EVENTTYPE_RESIZED:
            if (ev->window_width > 0 && ev->window_height > 0) {
                ctx.win_w = ev->framebuffer_width;
                ctx.win_h = ev->framebuffer_height;
                camera_resize(&ctx.core.cam, ctx.win_w, ctx.win_h);
                rebuild_texture();
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
            ctx.last_interaction_time = now;
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN:
            ie.type = INPUT_MOUSE_DOWN;
            ie.mouse_btn = (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            ctx.last_interaction_time = now;
            // let the settings panel consume the click first
            if (settings_panel_handle_mouse_down(&ctx.settings, &ctx.core,
                                                  ie.mouse_x, ie.mouse_y,
                                                  ctx.win_w, ctx.win_h)) {
                handled = 1;
            }
            break;

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            ie.type = INPUT_MOUSE_MOVE;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            // forward drag to settings panel slider
            if (settings_panel_handle_mouse_move(&ctx.settings, &ctx.core, ie.mouse_x)) {
                ctx.core.needs_redraw = 1;
                handled = 1;
                break;
            }
            if (ctx.core.cam.is_panning) {
                ctx.last_interaction_time = now;
            }
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            ie.type = INPUT_MOUSE_UP;
            ie.mouse_btn = (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            ie.mouse_x = (int)ev->mouse_x;
            ie.mouse_y = (int)ev->mouse_y;
            settings_panel_handle_mouse_up(&ctx.settings, &ctx.core);
            break;

        default:
            handled = 1;
            break;
    }

    if (!handled) {
        InputAction action = app_handle_input(&ctx.core, &ie, now);
        switch (action) {
            case ACTION_QUIT:
                sapp_request_quit();
                break;
            case ACTION_TOGGLE_SETTINGS:
                ctx.settings.visible = !ctx.settings.visible;
                app_state_push_notification(&ctx.core,
                    ctx.settings.visible ? "Settings: Open (drag slider | click palette)" : "Settings: Closed", now);
                ctx.core.needs_redraw = 1;
                break;
            case ACTION_TOGGLE_PERTURBATION:
                ctx.use_perturbation = !ctx.use_perturbation;
                ctx.core.needs_redraw = 1;
                app_state_push_notification(&ctx.core, ctx.use_perturbation ? "Perturbation: Enabled" : "Perturbation: Disabled", now);
                break;
            case ACTION_TOGGLE_GPU:
                ctx.gpu_mode = !ctx.gpu_mode;
                ctx.core.needs_redraw = 1;
                app_state_push_notification(&ctx.core, ctx.gpu_mode ? "Engine: GPU" : "Engine: CPU", now);
                break;
            case ACTION_TOGGLE_PRECISION:
                if (ctx.gpu_mode) {
                    ctx.high_precision_mode = !ctx.high_precision_mode;
                    app_state_push_notification(&ctx.core, ctx.high_precision_mode ? "Precision: 64-bit (Double)" : "Precision: 32-bit (Float)", now);
                } else {
#ifdef USE_SIMD_F128
                    ctx.cpu_precision_128 = !ctx.cpu_precision_128;
                    set_cpu_precision(ctx.renderer_ctx, ctx.cpu_precision_128);
                    app_state_push_notification(&ctx.core, ctx.cpu_precision_128 ? "Precision: 128-bit (SIMD)" : "Precision: 64-bit (Double)", now);
#endif
                }
                ctx.core.needs_redraw = 1;
                break;
            case ACTION_RELOAD_SHADERS:
                reload_shaders();
                ctx.core.needs_redraw = 1;
                break;
            case ACTION_MEGA_SCREENSHOT:
                if (!ctx.gpu_mode && ctx.core.mega_screenshot_active == 0) {
                    precise_float rmin, rmax, imin, imax;
                    app_state_calculate_boundaries(&ctx.core, ctx.win_w, ctx.win_h, &rmin, &rmax, &imin, &imax);
                    save_mega_screenshot_async(ctx.renderer_ctx, &ctx.core, 8192, 8192, rmin, rmax, imin, imax, ctx.core.max_iterations, ctx.core.palette_idx, ctx.core.julia_mode ? 1 : (ctx.core.base_fractal ? 2 : 0), ctx.core.julia_c);
                    ctx.core.needs_redraw = 1;
                    app_state_push_notification(&ctx.core, "Generating 8K Image...", now);
                }
                break;
            case ACTION_TOGGLE_VIDEO:
                if (is_video_recording()) {
                    stop_video_recording();
                    app_state_push_notification(&ctx.core, "Video Recording Saved!", now);
                } else {
                    if (start_video_recording(ctx.win_w, ctx.win_h, 60, 0)) {
                        app_state_push_notification(&ctx.core, "Video Recording Started", now);
                    } else {
                        app_state_push_notification(&ctx.core, "Error: ffmpeg not found!", now);
                    }
                }
                break;
            case ACTION_RESIZE_THREADS_UP:
            case ACTION_RESIZE_THREADS_DOWN: {
                int threads = get_actual_thread_count(ctx.renderer_ctx);
                threads += (action == ACTION_RESIZE_THREADS_UP) ? 1 : -1;
                set_renderer_thread_count(ctx.renderer_ctx, threads);
                ctx.core.thread_count = get_actual_thread_count(ctx.renderer_ctx);
                ctx.core.needs_redraw = 1;
                char buf[64];
                snprintf(buf, sizeof(buf), "CPU Threads: %d Cores", ctx.core.thread_count);
                app_state_push_notification(&ctx.core, buf, now);
                break;
            }
            default:
                if (ie.type == INPUT_KEY_DOWN && ie.key == KEY_S) {
                    ctx.screenshot_requested = 1;
                    ctx.core.needs_redraw = 1;
                }
                break;
        }
    }
}

static void cleanup(void) {
    free(ctx.pixels);
    free(ctx.capture_buf);
    free(ctx.flipped_buf);
    cleanup_renderer(ctx.renderer_ctx);
    cleanup_color_palette();
    sfons_destroy(ctx.fons);
    if (ctx.shd_cpu.id) sg_destroy_shader(ctx.shd_cpu);
    if (ctx.shd_gpu.id) sg_destroy_shader(ctx.shd_gpu);
    if (ctx.orbit_tex.id) sg_destroy_view(ctx.orbit_tex_view);
    if (ctx.orbit_tex.id) sg_destroy_image(ctx.orbit_tex);
    if (ctx.orbit_smp.id) sg_destroy_sampler(ctx.orbit_smp);
    if (ctx.dummy_img.id) sg_destroy_view(ctx.dummy_img_view);
    if (ctx.dummy_img.id) sg_destroy_image(ctx.dummy_img);
    if (ctx.dummy_smp.id) sg_destroy_sampler(ctx.dummy_smp);
    sgl_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    load_config_from_file("settings.txt");
    return (sapp_desc){.init_cb = init,
                       .frame_cb = frame,
                       .cleanup_cb = cleanup,
                       .event_cb = event,
                       .width = get_config_window_width(),
                       .height = get_config_window_height(),
                       .window_title = "Mandelbrot GPU Explorer",
                       .high_dpi = false};
}
