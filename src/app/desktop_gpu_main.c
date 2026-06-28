/* desktop_gpu_main.c
 *
 * high-performance gpu entry point using the sokol framework.
 * manages the graphics pipeline, gpgpu shader uniforms, and real-time interaction.
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
#ifdef BUILD_PERTURBATION
#include "perturbation.h"
#endif

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

#include "desktop_gpu_shaders.h"

#define JULIA_ZOOM 4.0

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

// gpu shader uniform block.
// mirrors the glsl layout exactly to ensure correct memory mapping.
typedef struct {
    float center_hi[2];
    float center_lo[2];
    float julia_c_hi[2];
    float julia_c_lo[2];
    float zoom, iters, aspect;
    float fractal_type, palette, high_precision;
#ifdef BUILD_PERTURBATION
    float use_perturbation;
    float orbit_len;
    float zoom_lo;  // low part of zoom for Dekker hi-lo in perturbation d0 calc
#endif
} params_t;

// application global context
typedef struct {
    // sokol gfx resources
    sg_pipeline pip_cpu, pip_gpu;
    sg_shader shd_cpu, shd_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
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

    // gpu specific modes
    int gpu_mode, high_precision_mode;
    int cpu_precision_128;
    int screenshot_requested;
    RendererContext* renderer_ctx;

    // debug ui and text rendering
    sgl_pipeline pip_blend;
    FONScontext* fons;
    int font_id;

#ifdef BUILD_PERTURBATION
    sg_image orbit_tex;
    sg_view orbit_tex_view;
    sg_sampler orbit_smp;
    int orbit_len;
    int use_perturbation;
    int active_perturbation_last; // cached result from orbit computation step
#endif
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

// screen coordinate conversion is handled by app_state

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
    // try to load shaders from files
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
#ifdef BUILD_PERTURBATION
        .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
        .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
        .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                     .view_slot = 0,
                                     .sampler_slot = 0,
                                     .glsl_name = "u_orbit"},
#endif
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
#ifdef BUILD_PERTURBATION
                {.glsl_name = "u_use_perturbation", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_orbit_len", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_zoom_lo", .type = SG_UNIFORMTYPE_FLOAT},
#endif
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

    // generate pipelines linked to new shaders
    ctx.pip_cpu =
        sg_make_pipeline(&(sg_pipeline_desc){.shader = ctx.shd_cpu,
                                             .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
                                             .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
                                             .index_type = SG_INDEXTYPE_UINT16});

    ctx.pip_gpu =
        sg_make_pipeline(&(sg_pipeline_desc){.shader = ctx.shd_gpu,
                                             .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
                                             .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
                                             .index_type = SG_INDEXTYPE_UINT16});

    free(vs_src);
    free(fs_cpu_src);
    free(fs_gpu_src);
    printf("shaders loaded/reloaded successfully.\n");
}

// initialization callback
static void init(void) {
    // initialize backend graphics APIs
    sg_setup(&(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
    sgl_desc_t sgl_desc = {0};
    sgl_setup(&sgl_desc);
    
    // 7. init hud font
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
    stm_setup();

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

    rebuild_texture();
    reload_shaders();
#ifdef BUILD_PERTURBATION
    ctx.orbit_tex = sg_make_image(&(sg_image_desc){
        .width = MAX_ITERATIONS_LIMIT,
        .height = 1,
        .pixel_format = SG_PIXELFORMAT_RG32F,
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
#endif

    ctx.pip_blend = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0].blend = {.enabled = true,
                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA}});

    init_fractal_registry();
    ctx.renderer_ctx = init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);

    // print controls console guide at startup
    puts("mandelbrot explorer (gpu)");
    puts("  left drag   : zoom selection   | right drag  : pan");
    puts("  scroll      : zoom at cursor   | ctrl+z      : undo");
    puts("  up/down     : iterations       | shift+up/dn : x100");
    puts("  p           : cycle palette    | r           : reset");
    puts("  e           : toggle precision (32/64-bit)");
    puts("  j           : julia mode       | t           : tour");
    puts("  f           : cycle fractals   | s           : screenshot");
    puts("  m           : save bookmark    | l           : load bookmark");
    puts("  x           : mega screenshot  | v           : record video");
    puts("  h           : toggle help menu | q / esc     : quit");
}

// frame rendering callback
static void frame(void) {
    uint32_t now = (uint32_t)stm_ms(stm_now());

    // step animation state machines
    app_state_update_tours(&ctx.core, now, set_window_title_cb);

    // force redraw to maintain steady frame pacing during video export
    if (is_video_recording()) {
        ctx.core.needs_redraw = 1;
    }

    // fallback: execute multi-threaded cpu render if gpu path is disabled
    if (!ctx.gpu_mode && ctx.core.needs_redraw && ctx.pixels) {
        set_cpu_precision(ctx.renderer_ctx, ctx.cpu_precision_128);
        precise_float rmin, rmax, im_top, im_bot;
        app_state_calculate_boundaries(&ctx.core, ctx.win_w, ctx.win_h, &rmin, &rmax, &im_bot,
                                       &im_top);
        uint32_t t0 = (uint32_t)stm_ms(stm_now());
        if (ctx.core.julia_mode)
            render_julia_threaded(ctx.renderer_ctx, ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                  im_top, im_bot, ctx.core.julia_c, ctx.core.max_iterations);
        else if (ctx.core.base_fractal)
            render_burning_ship_threaded(ctx.renderer_ctx, ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin,
                                         rmax, im_top, im_bot, ctx.core.max_iterations);
        else
            render_mandelbrot_threaded(ctx.renderer_ctx, ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                       im_top, im_bot, ctx.core.max_iterations);
        ctx.core.render_time_ms = (uint32_t)stm_ms(stm_now()) - t0;
        // swap red and blue channels for sokol sg_pixelformat_rgba8 texture upload
        int pixel_count = ctx.win_w * ctx.win_h;
        for (int i = 0; i < pixel_count; i++) {
            uint32_t pix = ctx.pixels[i];
            uint32_t r = (pix >> 16) & 0xFF;
            uint32_t b = pix & 0xFF;
            ctx.pixels[i] = (pix & 0xFF00FF00) | (b << 16) | r;
        }
        // upload staging buffer to gpu texture
        sg_update_image(ctx.img, &(sg_image_data){
                                     .mip_levels[0] = {.ptr = ctx.pixels,
                                                       .size = (size_t)ctx.win_w * ctx.win_h * 4}});
        ctx.core.needs_redraw = 0;
    }

#ifdef BUILD_PERTURBATION
    // minimum orbit length required for perturbation to give accurate results.
    // if the reference center escapes in fewer steps, the perturbation approximation
    // breaks down and we fall back to the dekker double-single rendering path.
    #define MIN_ORBIT_LEN 20

    // check if all conditions for perturbation mode are currently satisfied.
    int can_use_perturbation = ctx.gpu_mode && ctx.use_perturbation &&
                               (ctx.core.cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD) &&
                               !ctx.core.julia_mode && !ctx.core.base_fractal;

    if (!can_use_perturbation) {
        // mode changed (julia enabled, zoom out, etc.) — disable immediately.
        ctx.active_perturbation_last = 0;
    } else if (ctx.core.needs_redraw) {
        // view has moved — recompute the reference orbit and re-evaluate quality.
        precise_float center_re = ctx.core.cam.view.center_re;
        precise_float center_im = ctx.core.cam.view.center_im;
        int max_iters = ctx.core.max_iterations;
        if (max_iters > MAX_ITERATIONS_LIMIT) max_iters = MAX_ITERATIONS_LIMIT;

        RefOrbit* orb = perturbation_compute(center_re, center_im, max_iters);
        // debug: print orbit quality to terminal so we can see what's happening
        if (orb) {
            printf("[PERTURB] center=(%.6Lf, %.6Lf) orbit_len=%d min=%d -> %s\n",
                   (long double)center_re, (long double)center_im,
                   orb->len, MIN_ORBIT_LEN,
                   orb->len >= MIN_ORBIT_LEN ? "USE_PERTURB" : "FALLBACK");
            fflush(stdout);
        }
        if (orb && orb->len >= MIN_ORBIT_LEN) {
            // orbit is long enough — upload texture and enable perturbation.
            ctx.orbit_len = orb->len;
            ctx.active_perturbation_last = 1;

            static ComplexFloat orbit_upload_buf[MAX_ITERATIONS_LIMIT];
            for (int k = 0; k < orb->len; k++)
                orbit_upload_buf[k] = orb->zn[k];
            if (orb->len < MAX_ITERATIONS_LIMIT) {
                memset(orbit_upload_buf + orb->len, 0,
                       (MAX_ITERATIONS_LIMIT - orb->len) * sizeof(ComplexFloat));
            }
            sg_update_image(ctx.orbit_tex, &(sg_image_data){
                .mip_levels[0] = {.ptr = orbit_upload_buf,
                                  .size = sizeof(orbit_upload_buf)}});
        } else {
            // center escapes too quickly — perturbation is inaccurate here.
            // fall back to dekker double-single mode for this view position.
            ctx.active_perturbation_last = 0;
        }
        if (orb) perturbation_free(orb);
    }
#endif

    // background orchestration (drawing the fractal)
    sgl_viewport(0, 0, ctx.win_w, ctx.win_h, true);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)ctx.win_w, (float)ctx.win_h, 0.0f, -1.0f, 1.0f);

    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});

    if (ctx.gpu_mode) {
        sg_apply_pipeline(ctx.pip_gpu);

        int active_perturbation = 0;
#ifdef BUILD_PERTURBATION
        // reuse the decision made during orbit computation to ensure consistency
        active_perturbation = ctx.active_perturbation_last;
        if (active_perturbation) {
            ctx.bind.views[0] = ctx.orbit_tex_view;
            ctx.bind.samplers[0] = ctx.orbit_smp;
        } else {
            ctx.bind.views[0] = ctx.img_view;
            ctx.bind.samplers[0] = ctx.smp;
        }
#else
        ctx.bind.views[0] = ctx.img_view;
        ctx.bind.samplers[0] = ctx.smp;
#endif

        // calculate uniform parameters
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
        params.fractal_type =
            (float)(ctx.core.julia_mode ? 1 : ctx.core.base_fractal);
        params.palette = (float)ctx.core.palette_idx;
        params.high_precision = (float)ctx.high_precision_mode;
#ifdef BUILD_PERTURBATION
        params.use_perturbation = (float)active_perturbation;
        params.orbit_len = (float)ctx.orbit_len;
        // zoom_lo carries the sub-float residual of zoom for Dekker d0 computation
        // in the perturbation shader. allows d0 to be computed with ~14 decimal digits
        // of precision instead of the 7 offered by single-precision float alone.
        params.zoom_lo = (float)(ctx.core.cam.view.zoom - (precise_float)params.zoom);
#endif

        sg_apply_uniforms(0, &SG_RANGE(params));
    } else {
        ctx.bind.views[0] = ctx.img_view;
        ctx.bind.samplers[0] = ctx.smp;
        sg_apply_pipeline(ctx.pip_cpu);
    }
    sg_apply_bindings(&ctx.bind);
    sg_draw(0, 6, 1);

    // render interactive components using sokol_gl
    if (ctx.core.cam.is_zooming) {
        sgl_load_pipeline(ctx.pip_blend);
        sgl_begin_lines();
        sgl_c4b(255, 255, 0, 255);
        float x0 = (float)ctx.core.cam.zoom_rect.x;
        float y0 = (float)ctx.core.cam.zoom_rect.y;
        float x1 = x0 + (float)ctx.core.cam.zoom_rect.w;
        float y1 = y0 + (float)ctx.core.cam.zoom_rect.h;
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

    // render hud
    hud_render_sokol_gpu(ctx.fons, ctx.font_id, &ctx.core, ctx.win_w, ctx.win_h, ctx.gpu_mode,
                         ctx.high_precision_mode, ctx.cpu_precision_128,
#ifdef BUILD_PERTURBATION
                         ctx.active_perturbation_last, ctx.use_perturbation,
#else
                         0, 0,
#endif
                         ctx.pip_blend, now);

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
            // make sure we don't spin forever trying to take a screenshot if allocation fails
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
            case ACTION_QUIT:
                sapp_request_quit();
                break;
            case ACTION_TOGGLE_PERTURBATION:
#ifdef BUILD_PERTURBATION
                ctx.use_perturbation = !ctx.use_perturbation;
                ctx.core.needs_redraw = 1;
                app_state_push_notification(&ctx.core, ctx.use_perturbation ? "Perturbation: Enabled" : "Perturbation: Disabled", now);
#endif
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
#ifdef BUILD_PERTURBATION
    if (ctx.orbit_tex.id) sg_destroy_view(ctx.orbit_tex_view);
    if (ctx.orbit_tex.id) sg_destroy_image(ctx.orbit_tex);
    if (ctx.orbit_smp.id) sg_destroy_sampler(ctx.orbit_smp);
#endif
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
