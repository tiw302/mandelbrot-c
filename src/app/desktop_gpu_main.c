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
#define FONTSTASH_IMPLEMENTATION
#define SOKOL_FONTSTASH_IMPL

#include "app_state.h"
#include "bookmark.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "ini_config.h"
#include "julia.h"
#include "mandelbrot.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"

// clang-format off
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_gl.h"
#include "fons/fontstash.h"
#include "sokol/sokol_fontstash.h"
// clang-format on

// clang-format off
#if defined(__APPLE__)
#include <OpenGL/gl.h>
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

    // debug ui and text rendering
    sgl_pipeline pip_blend;
    FONScontext* fons;
    int font_id;
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
                              {.glsl_name = "u_high_precision", .type = SG_UNIFORMTYPE_FLOAT}}}});

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

    // setup fontstash for text rendering
    ctx.fons = sfons_create(&(sfons_desc_t){.width = 512, .height = 512});
    const char* font_paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                                FONT_PATH_3,     FONT_PATH_4, NULL};
    ctx.font_id = FONS_INVALID;
    for (int i = 0; font_paths[i] && font_paths[i][0]; i++) {
        ctx.font_id = fonsAddFont(ctx.fons, "sans", font_paths[i]);
        if (ctx.font_id != FONS_INVALID) break;
    }

    ctx.pip_blend = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0].blend = {.enabled = true,
                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA}});

    init_fractal_registry();
    init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
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
        set_cpu_precision(ctx.cpu_precision_128);
        precise_float rmin, rmax, im_top, im_bot;
        app_state_calculate_boundaries(&ctx.core, ctx.win_w, ctx.win_h, &rmin, &rmax, &im_bot,
                                       &im_top);
        uint32_t t0 = (uint32_t)stm_ms(stm_now());
        if (ctx.core.julia_mode)
            render_julia_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                  im_top, im_bot, ctx.core.julia_c, ctx.core.max_iterations);
        else if (ctx.core.burning_ship_mode)
            render_burning_ship_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin,
                                         rmax, im_top, im_bot, ctx.core.max_iterations);
        else
            render_mandelbrot_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
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

    // background orchestration (drawing the fractal)
    sgl_viewport(0, 0, ctx.win_w, ctx.win_h, true);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)ctx.win_w, (float)ctx.win_h, 0.0f, -1.0f, 1.0f);

    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});

    if (ctx.gpu_mode) {
        sg_apply_pipeline(ctx.pip_gpu);
        // calculate uniform parameters
        precise_float aspect = (precise_float)ctx.win_w / ctx.win_h;
        precise_float rmin = ctx.core.cam.view.center_re - ctx.core.cam.view.zoom * aspect / 2.0;
        precise_float im_bot = ctx.core.cam.view.center_im - ctx.core.cam.view.zoom / 2.0;

        params_t params = {0};
        params.center_hi[0] = (float)rmin;
        params.center_lo[0] = (float)(rmin - (precise_float)params.center_hi[0]);
        params.center_hi[1] = (float)im_bot;
        params.center_lo[1] = (float)(im_bot - (precise_float)params.center_hi[1]);
        params.julia_c_hi[0] = (float)ctx.core.julia_c.re;
        params.julia_c_lo[0] = (float)(ctx.core.julia_c.re - (precise_float)params.julia_c_hi[0]);
        params.julia_c_hi[1] = (float)ctx.core.julia_c.im;
        params.julia_c_lo[1] = (float)(ctx.core.julia_c.im - (precise_float)params.julia_c_hi[1]);
        params.zoom = (float)ctx.core.cam.view.zoom;
        params.iters = (float)ctx.core.max_iterations;
        params.aspect = (float)aspect;
        params.fractal_type =
            (float)(ctx.core.julia_mode ? 1 : (ctx.core.burning_ship_mode ? 2 : 0));
        params.palette = (float)ctx.core.palette_idx;
        params.high_precision = (float)ctx.high_precision_mode;

        sg_apply_uniforms(0, &SG_RANGE(params));
    } else {
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

    // render heads-up telemetry display
    if (DEBUG_INFO && ctx.font_id != FONS_INVALID) {
        sgl_load_pipeline(ctx.pip_blend);
        sgl_begin_quads();
        float visual_font_size = FONT_SIZE;
        float lh = visual_font_size + 6.0f;
        float bg_h = 3.0f * lh + 20.0f;
        if (ctx.core.m_tour.phase != TOUR_IDLE) bg_h += lh;
        float bg_w = 700.0f;

        sgl_c4b(20, 20, 25, 220);  // semi-transparent backdrop
        sgl_v2f(5.0f, 5.0f);
        sgl_v2f(bg_w, 5.0f);
        sgl_v2f(bg_w, bg_h);
        sgl_v2f(5.0f, bg_h);
        sgl_v2f(5.0f, 5.0f);
        sgl_end();

        // render text telemetry using fontstash
        fonsClearState(ctx.fons);
        fonsSetFont(ctx.fons, ctx.font_id);
        fonsSetSize(ctx.fons, visual_font_size);
        fonsSetAlign(ctx.fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        float x = 15.0f, y = 12.0f;
        char buf[256];

        // telemetry line 1
        fonsSetColor(ctx.fons, sfons_rgba(255, 255, 255, 255));
        const char* engine_name = "CPU (64-bit)";
        if (ctx.gpu_mode) {
            engine_name = ctx.high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
        } else {
#ifdef USE_SIMD_F128
            engine_name = ctx.cpu_precision_128 ? "CPU (128-bit)" : "CPU (64-bit)";
#endif
        }
        snprintf(buf, sizeof(buf), "[ENGINE] %s | Mode: %s | Threads: %d | Render: %u ms",
                 engine_name,
                 ctx.core.julia_mode ? "Julia"
                                     : (ctx.core.burning_ship_mode ? "Burning Ship" : "Mandelbrot"),
                 get_actual_thread_count(), ctx.core.render_time_ms);
        fonsDrawText(ctx.fons, x, y, buf, NULL);
        y += lh;

        // telemetry line 2
        if (ctx.core.julia_mode) {
            snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", ctx.core.julia_c.re,
                     ctx.core.julia_c.im);
        } else {
            snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)",
                     (double)ctx.core.cam.view.center_re, (double)ctx.core.cam.view.center_im);
        }
        fonsDrawText(ctx.fons, x, y, buf, NULL);
        y += lh;

        // telemetry line 3
        snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
                 (double)ctx.core.cam.view.zoom, ctx.core.max_iterations,
                 get_palette_name(ctx.core.palette_idx % get_palette_count()));
        fonsDrawText(ctx.fons, x, y, buf, NULL);
        y += lh;

        // tour telemetry
        if (ctx.core.m_tour.phase != TOUR_IDLE) {
            int t_idx = get_tour_target_idx(&ctx.core.m_tour);
            int t_tot = get_num_tour_targets(ctx.core.burning_ship_mode);
            double t_re = get_tour_target_re(&ctx.core.m_tour, ctx.core.burning_ship_mode);
            double t_im = get_tour_target_im(&ctx.core.m_tour, ctx.core.burning_ship_mode);
            snprintf(buf, sizeof(buf), "[TOUR]   Auto-Zoom [%s] Target #%d/%d (%.4f, %.4f)",
                     get_tour_phase_name(ctx.core.m_tour.phase), t_idx + 1, t_tot, t_re, t_im);
            fonsDrawText(ctx.fons, x, y, buf, NULL);
            y += lh;
        }

        sfons_flush(ctx.fons);
    }

    sgl_draw();

    // capture video frame or screenshot if requested
    if (is_video_recording() || ctx.screenshot_requested) {
        if (ensure_capture_buffers()) {
            glReadPixels(0, 0, ctx.win_w, ctx.win_h, GL_RGBA, GL_UNSIGNED_BYTE, ctx.capture_buf);
            for (int y = 0; y < ctx.win_h; y++) {
                int src_y = ctx.win_h - 1 - y;
                for (int x = 0; x < ctx.win_w; x++) {
                    uint32_t p = ctx.capture_buf[src_y * ctx.win_w + x];
                    uint8_t r = (p >> 0) & 0xFF;
                    uint8_t g = (p >> 8) & 0xFF;
                    uint8_t b = (p >> 16) & 0xFF;
                    uint8_t a = (p >> 24) & 0xFF;
                    ctx.flipped_buf[y * ctx.win_w + x] = (uint32_t)b | ((uint32_t)g << 8) |
                                                         ((uint32_t)r << 16) | ((uint32_t)a << 24);
                }
            }
            if (is_video_recording()) {
                append_video_frame(ctx.flipped_buf, ctx.win_w, ctx.win_h);
            }
            if (ctx.screenshot_requested) {
                save_screenshot(ctx.flipped_buf, ctx.win_w, ctx.win_h);
                ctx.screenshot_requested = 0;
            }
        } else {
            // make sure we don't spin forever trying to take a screenshot if allocation fails
            ctx.screenshot_requested = 0;
        }
    }

    sg_end_pass();
    sg_commit();
}

static void handle_mouse(const sapp_event* event) {
    switch (event->type) {
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            camera_handle_wheel(&ctx.core.cam, (double)event->scroll_y, ctx.core.cam.mouse_x,
                                ctx.core.cam.mouse_y);
            ctx.core.needs_redraw = 1;
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN: {
            int btn = (event->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            camera_handle_mouse_down(&ctx.core.cam, btn, (int)event->mouse_x, (int)event->mouse_y);
            break;
        }

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            camera_handle_mouse_motion(&ctx.core.cam, (int)event->mouse_x, (int)event->mouse_y);
            if (ctx.core.cam.is_panning) {
                ctx.core.needs_redraw = 1;
            } else if (ctx.core.julia_mode && !ctx.core.cam.is_zooming) {
                app_state_get_mouse_coord(&ctx.core, ctx.core.cam.mouse_x, ctx.core.cam.mouse_y,
                                          &ctx.core.julia_c.re, &ctx.core.julia_c.im);
                ctx.core.needs_redraw = 1;
            }
            break;

        case SAPP_EVENTTYPE_MOUSE_UP: {
            int btn = (event->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 3 : 1;
            if (camera_handle_mouse_up(&ctx.core.cam, btn)) {
                ctx.core.needs_redraw = 1;
            }
            break;
        }

        default:
            break;
    }
}

static void handle_keydown(const sapp_event* event) {
    if (event->key_code == SAPP_KEYCODE_ESCAPE || event->key_code == SAPP_KEYCODE_Q) {
        sapp_request_quit();
    } else if (event->key_code == SAPP_KEYCODE_Z && (event->modifiers & SAPP_MODIFIER_CTRL)) {
        if (camera_pop_history(&ctx.core.cam)) {
            ctx.core.needs_redraw = 1;
        }
    } else if (event->key_code == SAPP_KEYCODE_R) {
        app_state_reset(&ctx.core, set_window_title_cb);
    } else if (event->key_code == SAPP_KEYCODE_G) {
        ctx.gpu_mode = !ctx.gpu_mode;
        ctx.core.needs_redraw = 1;
    } else if (event->key_code == SAPP_KEYCODE_E) {
        if (ctx.gpu_mode)
            ctx.high_precision_mode = !ctx.high_precision_mode;
        else {
#ifdef USE_SIMD_F128
            ctx.cpu_precision_128 = !ctx.cpu_precision_128;
            set_cpu_precision(ctx.cpu_precision_128);
#endif
        }
        ctx.core.needs_redraw = 1;
    } else if (event->key_code == SAPP_KEYCODE_P) {
        app_state_cycle_palette(&ctx.core);
    } else if (event->key_code == SAPP_KEYCODE_J) {
        app_state_toggle_julia(&ctx.core, set_window_title_cb);
    } else if (event->key_code == SAPP_KEYCODE_B) {
        app_state_toggle_burning_ship(&ctx.core, set_window_title_cb);
    } else if (event->key_code == SAPP_KEYCODE_UP || event->key_code == SAPP_KEYCODE_DOWN) {
        int step = ctx.core.max_iterations / 10;
        if (step < 10) step = 10;
        if (event->modifiers & SAPP_MODIFIER_SHIFT) step *= 10;
        ctx.core.max_iterations += (event->key_code == SAPP_KEYCODE_UP) ? step : -step;
        if (ctx.core.max_iterations < 10) ctx.core.max_iterations = 10;
        if (ctx.core.max_iterations > get_config_max_iterations_limit())
            ctx.core.max_iterations = get_config_max_iterations_limit();
        init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
        ctx.core.needs_redraw = 1;
    } else if (event->key_code == SAPP_KEYCODE_S) {
        ctx.screenshot_requested = 1;
    } else if (event->key_code == SAPP_KEYCODE_X) {
        if (!ctx.gpu_mode) {
            precise_float rmin, rmax, imin, imax;
            app_state_calculate_boundaries(&ctx.core, ctx.win_w, ctx.win_h, &rmin, &rmax, &imin,
                                           &imax);
            save_mega_screenshot(
                8192, 8192, rmin, rmax, imin, imax, ctx.core.max_iterations, ctx.core.palette_idx,
                ctx.core.julia_mode ? 1 : (ctx.core.burning_ship_mode ? 2 : 0), ctx.core.julia_c);
            ctx.core.needs_redraw = 1;
        }
    } else if (event->key_code == SAPP_KEYCODE_V) {
        if (is_video_recording())
            stop_video_recording();
        else
            start_video_recording(ctx.win_w, ctx.win_h, 60);
    } else if (event->key_code == SAPP_KEYCODE_M) {
        app_state_save_bookmark(&ctx.core);
    } else if (event->key_code == SAPP_KEYCODE_L) {
        app_state_load_next_bookmark(&ctx.core);
    } else if (event->key_code == SAPP_KEYCODE_T) {
        app_state_toggle_tour(&ctx.core, (uint32_t)stm_ms(stm_now()), set_window_title_cb);
    } else if (event->key_code == SAPP_KEYCODE_LEFT_BRACKET ||
               event->key_code == SAPP_KEYCODE_RIGHT_BRACKET) {
        int threads = get_actual_thread_count();
        threads += (event->key_code == SAPP_KEYCODE_RIGHT_BRACKET) ? 1 : -1;
        set_renderer_thread_count(threads);
        ctx.core.needs_redraw = 1;
    } else if (event->key_code == SAPP_KEYCODE_F5) {
        reload_shaders();
        ctx.core.needs_redraw = 1;
    }
}

// handles window events and input mapping
static void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        if (ev->window_width > 0 && ev->window_height > 0) {
            ctx.win_w = ev->framebuffer_width;
            ctx.win_h = ev->framebuffer_height;
            camera_resize(&ctx.core.cam, ctx.win_w, ctx.win_h);
            rebuild_texture();
            ctx.core.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN || ev->type == SAPP_EVENTTYPE_MOUSE_UP ||
               ev->type == SAPP_EVENTTYPE_MOUSE_MOVE || ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        handle_mouse(ev);
    } else if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        handle_keydown(ev);
    }
}

static void cleanup(void) {
    free(ctx.pixels);
    free(ctx.capture_buf);
    free(ctx.flipped_buf);
    cleanup_renderer();
    cleanup_color_palette();
    sfons_destroy(ctx.fons);
    if (ctx.shd_cpu.id) sg_destroy_shader(ctx.shd_cpu);
    if (ctx.shd_gpu.id) sg_destroy_shader(ctx.shd_gpu);
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
