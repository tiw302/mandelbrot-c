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

#include "bookmark.h"
#include "color.h"
#include "config.h"
#include "ini_config.h"
#include "julia.h"
#include "mandelbrot.h"
#include "renderer.h"
#include "screenshot.h"

// clang-format off
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_gl.h"
#include "fons/fontstash.h"
#include "sokol/sokol_fontstash.h"
// clang-format on

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "desktop_gpu_shaders.h"
#include "tour.h"

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

// navigation state to restore when toggling modes
typedef struct {
    ViewState mandelbrot_view;
    int active;
} JuliaSession;

// application global context
typedef struct {
    // sokol gfx resources
    sg_pipeline pip_cpu, pip_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t* pixels;  // staging buffer for cpu-mode texture uploads
    int win_w, win_h;

    // navigation state
    ViewState view;
    ViewState history[MAX_HISTORY_SIZE];
    int history_count;

    // runtime mode flags
    TourState m_tour;
    JuliaTourState j_tour;
    int julia_mode, gpu_mode, high_precision_mode, burning_ship_mode;
    int cpu_precision_128;
    complex_t julia_c;
    JuliaSession julia_session;

    // renderer parameters
    int max_iterations, palette_idx;

    // bookmark tracking
    int current_bookmark_idx;
    int needs_redraw;

    // mouse and interaction state
    int is_panning, is_zooming;
    int last_mouse_x, last_mouse_y;
    int mouse_x, mouse_y;
    struct {
        int x, y, w, h;
    } zoom_rect;
    uint32_t render_time_ms;

    // debug ui and text rendering
    sgl_pipeline pip_blend;
    FONScontext* fons;
    int font_id;
} GlobalCtx;

static GlobalCtx ctx;

// re-allocates backend texture and staging buffer on window resize
static void rebuild_texture(void) {
    if (ctx.img.id) sg_destroy_view(ctx.img_view);
    if (ctx.img.id) sg_destroy_image(ctx.img);
    free(ctx.pixels);
    ctx.pixels = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * 4);
    ctx.img = sg_make_image(&(sg_image_desc){.width = ctx.win_w,
                                             .height = ctx.win_h,
                                             .pixel_format = SG_PIXELFORMAT_RGBA8,
                                             .usage = {.dynamic_update = true}});
    ctx.img_view = sg_make_view(&(sg_view_desc){.texture.image = ctx.img});
    ctx.bind.views[0] = ctx.img_view;
}

// transforms current mouse coordinates to the complex plane
static double mouse_re(void) {
    return ctx.view.center_re + ((double)ctx.mouse_x / ctx.win_w - 0.5) * ctx.view.zoom *
                                    ((double)ctx.win_w / ctx.win_h);
}
static double mouse_im(void) {
    return ctx.view.center_im + (0.5 - (double)ctx.mouse_y / ctx.win_h) * ctx.view.zoom;
}

// initializes graphics API, shaders, and application state
static void init(void) {
    sg_setup(&(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
    stm_setup();
    sgl_setup(&(sgl_desc_t){.logger.func = slog_func});

    // setup blending for translucent hud elements
    ctx.pip_blend = sgl_make_pipeline(
        &(sg_pipeline_desc){.colors[0].blend = {
                                .enabled = true,
                                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                            }});

    // bootstrap font system for hud overlays
    ctx.fons = sfons_create(&(sfons_desc_t){.width = 512, .height = 512});
    const char* font_paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                                FONT_PATH_3,     FONT_PATH_4, NULL};
    ctx.font_id = FONS_INVALID;
    for (int i = 0; font_paths[i] && font_paths[i][0]; i++) {
        ctx.font_id = fonsAddFont(ctx.fons, "sans", font_paths[i]);
        if (ctx.font_id != FONS_INVALID) break;
    }

    ctx.win_w = sapp_width();
    ctx.win_h = sapp_height();

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

    // build cpu fallback pipeline (sampling a texture)
    sg_shader shd_cpu = sg_make_shader(
        &(sg_shader_desc){.attrs[0].glsl_name = "pos",
                          .attrs[1].glsl_name = "uv_in",
                          .vertex_func.source = dg_vs,
                          .fragment_func.source = dg_fs_cpu,
                          .views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT,
                          .samplers[0].stage = SG_SHADERSTAGE_FRAGMENT,
                          .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                                       .view_slot = 0,
                                                       .sampler_slot = 0,
                                                       .glsl_name = "tex"}});
    ctx.pip_cpu =
        sg_make_pipeline(&(sg_pipeline_desc){.shader = shd_cpu,
                                             .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
                                             .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
                                             .index_type = SG_INDEXTYPE_UINT16});

    // build gpu compute pipeline (calculating per-pixel in shader)
    sg_shader shd_gpu = sg_make_shader(&(sg_shader_desc){
        .attrs[0].glsl_name = "pos",
        .attrs[1].glsl_name = "uv_in",
        .vertex_func.source = dg_vs,
        .fragment_func.source = dg_fs_gpu,
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
    ctx.pip_gpu =
        sg_make_pipeline(&(sg_pipeline_desc){.shader = shd_gpu,
                                             .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
                                             .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
                                             .index_type = SG_INDEXTYPE_UINT16});

    ctx.pass_action = (sg_pass_action){
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0, 0, 0, 1}}};

    // application state init
    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.julia_c = (complex_t){-0.7, 0.27};
    ctx.max_iterations = get_config_default_iterations();
    ctx.palette_idx = get_config_default_palette();
    ctx.gpu_mode = 1;
    ctx.cpu_precision_128 = 0;
    ctx.m_tour = (TourState){TOUR_IDLE, 0, 0, 0, 0, 0, 0, 0, -1};
    ctx.j_tour = (JuliaTourState){JULIA_TOUR_IDLE, 0, 0, 0, 0, 0, -1};
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    set_cpu_precision(ctx.cpu_precision_128);
    ctx.needs_redraw = 1;

    srand((unsigned)time(NULL));

    puts("mandelbrot explorer");
    puts("  left drag   : zoom selection   | right drag  : pan");
    puts("  scroll      : zoom at cursor   | ctrl+z      : undo");
    puts("  up/down     : iterations       | shift+up/dn : x100");
    puts("  p           : cycle palette    | r           : reset");
    puts("  e           : toggle precision (64/128-bit)");
    puts("  j           : julia mode       | t           : tour");
    puts("  b           : burning ship     | s           : screenshot");
    puts("  m           : save bookmark    | l           : load bookmark");
    puts("  x           : mega screenshot  | v           : record video");
    puts("  q / esc     : quit");
}

// logic and rendering dispatched once per frame
static void frame(void) {
    uint32_t now = (uint32_t)stm_ms(stm_now());

    // step animation state machines
    if (ctx.m_tour.phase != TOUR_IDLE) {
        update_tour(&ctx.m_tour, &ctx.view, now, ctx.burning_ship_mode);
        ctx.needs_redraw = 1;
    }
    if (ctx.j_tour.phase != JULIA_TOUR_IDLE) {
        update_julia_tour(&ctx.j_tour, &ctx.julia_c, now);
        ctx.needs_redraw = 1;
    }

    // fallback: execute multi-threaded cpu render if gpu path is disabled
    if (!ctx.gpu_mode && ctx.needs_redraw) {
        set_cpu_precision(ctx.cpu_precision_128);
        double aspect = (double)ctx.win_w / ctx.win_h;
        double rmin = ctx.view.center_re - ctx.view.zoom * aspect / 2;
        double rmax = ctx.view.center_re + ctx.view.zoom * aspect / 2;
        double im_top = ctx.view.center_im + ctx.view.zoom / 2;
        double im_bot = ctx.view.center_im - ctx.view.zoom / 2;
        uint32_t t0 = (uint32_t)stm_ms(stm_now());
        if (ctx.julia_mode)
            render_julia_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                  im_top, im_bot, ctx.julia_c, ctx.max_iterations);
        else if (ctx.burning_ship_mode)
            render_burning_ship_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin,
                                         rmax, im_top, im_bot, ctx.max_iterations);
        else
            render_mandelbrot_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h, rmin, rmax,
                                       im_top, im_bot, ctx.max_iterations);
        ctx.render_time_ms = (uint32_t)stm_ms(stm_now()) - t0;
        // upload staging buffer to gpu texture
        sg_update_image(ctx.img, &(sg_image_data){
                                     .mip_levels[0] = {.ptr = ctx.pixels,
                                                       .size = (size_t)ctx.win_w * ctx.win_h * 4}});
        ctx.needs_redraw = 0;
    }

    // background orchestration (drawing the fractal)
    sgl_viewport(0, 0, ctx.win_w, ctx.win_h, true);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)ctx.win_w, (float)ctx.win_h, 0.0f, -1.0f, 1.0f);

    // render interactive zoom box
    if (ctx.is_zooming && ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
        float x1 = (float)ctx.zoom_rect.x;
        float y1 = (float)ctx.zoom_rect.y;
        float x2 = (float)(ctx.zoom_rect.x + ctx.zoom_rect.w);
        float y2 = (float)(ctx.zoom_rect.y + ctx.zoom_rect.h);
        sgl_begin_lines();
        sgl_c3b(255, 255, 0);
        sgl_v2f(x1, y1);
        sgl_v2f(x2, y1);
        sgl_v2f(x2, y1);
        sgl_v2f(x2, y2);
        sgl_v2f(x2, y2);
        sgl_v2f(x1, y2);
        sgl_v2f(x1, y2);
        sgl_v2f(x1, y1);
        sgl_end();
    }
    sgl_load_default_pipeline();

    // main rendering pass
    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});
    sg_pipeline cur = ctx.gpu_mode ? ctx.pip_gpu : ctx.pip_cpu;
    if (sg_query_pipeline_state(cur) == SG_RESOURCESTATE_VALID) {
        sg_apply_pipeline(cur);
        sg_apply_bindings(&ctx.bind);
        // build uniform data for gpu path
        if (ctx.gpu_mode) {
            float chi_re = (float)ctx.view.center_re;
            float chi_im = (float)ctx.view.center_im;
            float jc_re = (float)ctx.julia_c.re;
            float jc_im = (float)ctx.julia_c.im;
            params_t p = {
                // hi-lo split for simulated 64-bit precision
                .center_hi = {chi_re, chi_im},
                .center_lo = {(float)(ctx.view.center_re - chi_re),
                              (float)(ctx.view.center_im - chi_im)},
                .julia_c_hi = {jc_re, jc_im},
                .julia_c_lo = {(float)(ctx.julia_c.re - jc_re), (float)(ctx.julia_c.im - jc_im)},
                .zoom = (float)ctx.view.zoom,
                .iters = (float)ctx.max_iterations,
                .aspect = (float)ctx.win_w / ctx.win_h,
                .fractal_type = ctx.julia_mode ? 1.0f : (ctx.burning_ship_mode ? 2.0f : 0.0f),
                .palette = (float)ctx.palette_idx,
                .high_precision = ctx.high_precision_mode ? 1.0f : 0.0f};
            sg_apply_uniforms(0, &SG_RANGE(p));
        }
        sg_draw(0, 6, 1);
    }

    // hud compositing
    sgl_load_pipeline(ctx.pip_blend);
    int num_lines = 3;
    if (ctx.m_tour.phase != TOUR_IDLE) num_lines++;
    if (ctx.j_tour.phase != JULIA_TOUR_IDLE) num_lines++;

    float visual_font_size = FONT_SIZE * 1.26f;
    float lh = visual_font_size + 6.0f;
    float bg_w = 780.0f;
    float bg_h = num_lines * lh + 20.0f;
    sgl_begin_quads();
    sgl_c4b(20, 20, 25, 220);  // semi-transparent backdrop
    sgl_v2f(5.0f, 5.0f);
    sgl_v2f(bg_w, 5.0f);
    sgl_v2f(bg_w, bg_h);
    sgl_v2f(5.0f, bg_h);
    sgl_end();

    // render text telemetry using fontstash
    if (ctx.font_id != FONS_INVALID) {
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
                 ctx.julia_mode ? "Julia" : (ctx.burning_ship_mode ? "Burning Ship" : "Mandelbrot"),
                 get_actual_thread_count(), ctx.render_time_ms);
        fonsDrawText(ctx.fons, x, y, buf, NULL);
        y += lh;

        // telemetry line 2
        if (ctx.julia_mode) {
            snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", ctx.julia_c.re,
                     ctx.julia_c.im);
        } else {
            snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)", ctx.view.center_re,
                     ctx.view.center_im);
        }
        fonsDrawText(ctx.fons, x, y, buf, NULL);
        y += lh;

        // telemetry line 3
        snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s", ctx.view.zoom,
                 ctx.max_iterations, PALETTE_NAMES[ctx.palette_idx % PALETTE_COUNT]);
        fonsDrawText(ctx.fons, x, y, buf, NULL);
        y += lh;

        // tour telemetry
        if (ctx.m_tour.phase != TOUR_IDLE) {
            int t_idx = get_tour_target_idx(&ctx.m_tour);
            int t_tot = get_num_tour_targets(ctx.burning_ship_mode);
            double t_re = get_tour_target_re(&ctx.m_tour, ctx.burning_ship_mode);
            double t_im = get_tour_target_im(&ctx.m_tour, ctx.burning_ship_mode);
            snprintf(buf, sizeof(buf), "[TOUR]   Auto-Zoom [%s] Target #%d/%d (%.4f, %.4f)",
                     get_tour_phase_name(ctx.m_tour.phase), t_idx + 1, t_tot, t_re, t_im);
            fonsDrawText(ctx.fons, x, y, buf, NULL);
            y += lh;
        }

        sfons_flush(ctx.fons);
    }

    sgl_draw();
    sg_end_pass();
    sg_commit();
}

// handles window events and input mapping
static void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        ctx.win_w = ev->window_width;
        ctx.win_h = ev->window_height;
        rebuild_texture();
        ctx.needs_redraw = 1;
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
        // user interaction cancels automated navigation
        if (ctx.m_tour.phase != TOUR_IDLE) {
            stop_tour(&ctx.m_tour);
            sapp_set_window_title("Mandelbrot GPU Explorer");
            ctx.needs_redraw = 1;
        }
        if (ctx.j_tour.phase != JULIA_TOUR_IDLE) {
            stop_julia_tour(&ctx.j_tour);
            sapp_set_window_title("Julia Explorer");
            ctx.needs_redraw = 1;
        }
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
            ctx.is_panning = 1;
            ctx.last_mouse_x = (int)ev->mouse_x;
            ctx.last_mouse_y = (int)ev->mouse_y;
        } else if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            ctx.is_zooming = 1;
            ctx.zoom_rect.x = (int)ev->mouse_x;
            ctx.zoom_rect.y = (int)ev->mouse_y;
            ctx.zoom_rect.w = ctx.zoom_rect.h = 0;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP) {
        if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
            ctx.is_panning = 0;
        } else if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            // apply box zoom
            if (ctx.is_zooming && ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
                if (ctx.history_count < MAX_HISTORY_SIZE)
                    ctx.history[ctx.history_count++] = ctx.view;
                double aspect = (double)ctx.win_w / ctx.win_h;
                double re_pp = ctx.view.zoom * aspect / ctx.win_w;
                double im_pp = ctx.view.zoom / ctx.win_h;
                int zx = ctx.zoom_rect.w > 0 ? ctx.zoom_rect.x : ctx.zoom_rect.x + ctx.zoom_rect.w;
                int zy = ctx.zoom_rect.h > 0 ? ctx.zoom_rect.y : ctx.zoom_rect.y + ctx.zoom_rect.h;
                int zw = abs(ctx.zoom_rect.w), zh = abs(ctx.zoom_rect.h);
                double re_min = ctx.view.center_re - ctx.view.zoom * aspect / 2.0;
                double im_max = ctx.view.center_im + ctx.view.zoom / 2.0;
                ctx.view.center_re = re_min + (zx + zw / 2.0) * re_pp;
                ctx.view.center_im = im_max - (zy + zh / 2.0) * im_pp;
                double target_zoom = fmax(zw * re_pp, zh * im_pp);
                double min_zoom = (ctx.gpu_mode ? 1e-15 : (ctx.cpu_precision_128 ? 1e-30 : 1e-15));
                ctx.view.zoom = fmax(target_zoom, min_zoom);
                ctx.needs_redraw = 1;
            }
            ctx.is_zooming = 0;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        ctx.mouse_x = (int)ev->mouse_x;
        ctx.mouse_y = (int)ev->mouse_y;
        if (ctx.is_panning) {
            double aspect = (double)ctx.win_w / ctx.win_h;
            ctx.view.center_re -=
                (ev->mouse_x - ctx.last_mouse_x) * ctx.view.zoom * aspect / ctx.win_w;
            ctx.view.center_im += (ev->mouse_y - ctx.last_mouse_y) * ctx.view.zoom / ctx.win_h;
            ctx.last_mouse_x = (int)ev->mouse_x;
            ctx.last_mouse_y = (int)ev->mouse_y;
            ctx.needs_redraw = 1;
        } else if (ctx.is_zooming) {
            ctx.zoom_rect.w = (int)ev->mouse_x - ctx.zoom_rect.x;
            ctx.zoom_rect.h = (int)ev->mouse_y - ctx.zoom_rect.y;
        } else if (ctx.julia_mode && ctx.j_tour.phase == JULIA_TOUR_IDLE) {
            // update julia parameter c based on mouse position
            ctx.julia_c.re = mouse_re();
            ctx.julia_c.im = mouse_im();
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        if (ev->scroll_y != 0.0f) {
            if (ctx.history_count < MAX_HISTORY_SIZE) ctx.history[ctx.history_count++] = ctx.view;
            double factor = ev->scroll_y > 0 ? 0.9 : 1.1;
            double mre = mouse_re(), mim = mouse_im();
            ctx.view.zoom *= factor;
            double aspect = (double)ctx.win_w / ctx.win_h;
            // scale view relative to the cursor focal point
            ctx.view.center_re =
                mre - ((double)ctx.mouse_x / ctx.win_w - 0.5) * ctx.view.zoom * aspect;
            ctx.view.center_im = mim - (0.5 - (double)ctx.mouse_y / ctx.win_h) * ctx.view.zoom;
            ctx.needs_redraw = 1;
        }
    } else if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        int mod_ctrl = ev->modifiers & SAPP_MODIFIER_CTRL;
        if (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_Q) {
            sapp_quit();
        } else if (ev->key_code == SAPP_KEYCODE_Z && mod_ctrl) {
            if (ctx.history_count > 0) ctx.view = ctx.history[--ctx.history_count];
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_R) {
            ctx.julia_mode = 0;
            ctx.m_tour.phase = TOUR_IDLE;
            ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
            ctx.history_count = 0;
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_G) {
            ctx.gpu_mode = !ctx.gpu_mode;
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_E) {
            if (ctx.gpu_mode)
                ctx.high_precision_mode = !ctx.high_precision_mode;
            else {
#ifdef USE_SIMD_F128
                ctx.cpu_precision_128 = !ctx.cpu_precision_128;
                set_cpu_precision(ctx.cpu_precision_128);
#endif
            }
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_P) {
            ctx.palette_idx = (ctx.palette_idx + 1) % PALETTE_COUNT;
            init_renderer(ctx.max_iterations, ctx.palette_idx);
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_J) {
            if (!ctx.julia_mode) {
                ctx.julia_session.mandelbrot_view = ctx.view;
                ctx.julia_session.active = 1;
                ctx.julia_c.re = mouse_re();
                ctx.julia_c.im = mouse_im();
                ctx.view = (ViewState){0.0, 0.0, JULIA_ZOOM};
                ctx.julia_mode = 1;
                ctx.history_count = 0;
            } else {
                if (ctx.julia_session.active) ctx.view = ctx.julia_session.mandelbrot_view;
                ctx.julia_mode = 0;
            }
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_B) {
            ctx.burning_ship_mode = !ctx.burning_ship_mode;
            ctx.julia_mode = ctx.julia_session.active = 0;
            ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
            ctx.history_count = 0;
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_UP || ev->key_code == SAPP_KEYCODE_DOWN) {
            int step = ctx.max_iterations / 10;
            if (step < 10) step = 10;
            if (ev->modifiers & SAPP_MODIFIER_SHIFT) step *= 10;
            ctx.max_iterations += (ev->key_code == SAPP_KEYCODE_UP) ? step : -step;
            if (ctx.max_iterations < 10) ctx.max_iterations = 10;
            if (ctx.max_iterations > get_config_max_iterations_limit())
                ctx.max_iterations = get_config_max_iterations_limit();
            init_renderer(ctx.max_iterations, ctx.palette_idx);
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_S) {
            uint32_t* ss = malloc((size_t)ctx.win_w * ctx.win_h * 4);
            if (ss) {
                // unfortunately glReadPixels is tricky in sokol's frame loop,
                // we'd normally use a flag and read in frame(), but for simplicity
                // in this desktop-only code we can use glReadPixels if we know the context.
                glReadPixels(0, 0, ctx.win_w, ctx.win_h, GL_RGBA, GL_UNSIGNED_BYTE, ss);
                save_screenshot(ss, ctx.win_w, ctx.win_h);
                free(ss);
            }
        } else if (ev->key_code == SAPP_KEYCODE_X) {
            double aspect = (double)ctx.win_w / ctx.win_h;
            double rmin = ctx.view.center_re - ctx.view.zoom * aspect / 2.0;
            double rmax = ctx.view.center_re + ctx.view.zoom * aspect / 2.0;
            double im_top = ctx.view.center_im + ctx.view.zoom / 2.0;
            double im_bot = ctx.view.center_im - ctx.view.zoom / 2.0;
            save_mega_screenshot(8192, 8192, rmin, rmax, im_bot, im_top, ctx.max_iterations,
                                 ctx.palette_idx,
                                 ctx.julia_mode ? 1 : (ctx.burning_ship_mode ? 2 : 0), ctx.julia_c);
            ctx.needs_redraw = 1;
        } else if (ev->key_code == SAPP_KEYCODE_V) {
            if (is_video_recording())
                stop_video_recording();
            else
                start_video_recording(ctx.win_w, ctx.win_h, 60);
        } else if (ev->key_code == SAPP_KEYCODE_M) {
            Bookmark b = {ctx.view.center_re,
                          ctx.view.center_im,
                          ctx.view.zoom,
                          ctx.max_iterations,
                          ctx.julia_mode ? 1 : (ctx.burning_ship_mode ? 2 : 0),
                          ctx.julia_c};
            save_bookmark(&b);
        } else if (ev->key_code == SAPP_KEYCODE_L) {
            Bookmark b;
            int count = get_bookmark_count();
            if (count > 0) {
                if (ctx.history_count < MAX_HISTORY_SIZE)
                    ctx.history[ctx.history_count++] = ctx.view;
                ctx.current_bookmark_idx = (ctx.current_bookmark_idx + 1) % count;
                if (load_bookmark(ctx.current_bookmark_idx, &b)) {
                    ctx.view = (ViewState){b.center_re, b.center_im, b.zoom};
                    ctx.max_iterations = b.max_iterations;
                    ctx.julia_mode = (b.fractal_type == 1);
                    ctx.burning_ship_mode = (b.fractal_type == 2);
                    ctx.julia_c = b.julia_c;
                    init_renderer(ctx.max_iterations, ctx.palette_idx);
                    ctx.needs_redraw = 1;
                }
            }
        } else if (ev->key_code == SAPP_KEYCODE_T) {
            if (ctx.julia_mode) {
                if (ctx.j_tour.phase == JULIA_TOUR_IDLE) {
                    start_julia_tour(&ctx.j_tour, &ctx.julia_c, (uint32_t)stm_ms(stm_now()));
                    sapp_set_window_title("Julia Explorer [Auto-c]");
                } else {
                    stop_julia_tour(&ctx.j_tour);
                    sapp_set_window_title("Julia Explorer");
                    ctx.needs_redraw = 1;
                }
            } else {
                if (ctx.m_tour.phase == TOUR_IDLE) {
                    ctx.julia_mode = ctx.julia_session.active = 0;
                    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    start_tour(&ctx.m_tour, &ctx.view);
                    sapp_set_window_title("Mandelbrot Explorer [Auto-Zoom]");
                } else {
                    stop_tour(&ctx.m_tour);
                    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    sapp_set_window_title("Mandelbrot Explorer");
                    ctx.needs_redraw = 1;
                }
                ctx.history_count = 0;
            }
        }
    }
}

static void cleanup(void) {
    free(ctx.pixels);
    cleanup_renderer();
    cleanup_color_palette();
    sfons_destroy(ctx.fons);
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
