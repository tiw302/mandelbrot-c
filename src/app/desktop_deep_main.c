/* desktop_deep_main.c
 *
 * high-performance deep-zoom mandelbrot renderer using gpu perturbation theory.
 * runs as a minimal standalone binary using sokol for fast iteration.
 */

#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL
#define SOKOL_GL_IMPL
#include "app_state.h"
#include "tour.h"
#include "input_handler.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "renderer.h"
#include "screenshot.h"
#include "perturbation.h"
#include "hud_sokol.h"
#include "ini_config.h"

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "desktop_gpu_shaders.h"

// gpu shader uniform block.
// mirrors the glsl layout exactly to ensure correct memory mapping.
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
} params_t;

// application global context
typedef struct {
    sg_pipeline pip_gpu;
    sg_shader shd_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image dummy_img; // dummy texture bound when perturbation is active
    sg_view dummy_img_view;
    sg_sampler dummy_smp;
    int win_w, win_h;

    // persistent buffers for screenshot capture
    uint32_t* capture_buf;
    int capture_w, capture_h;

    // common application state
    AppCommonState core;

    int high_precision_mode;
    int screenshot_requested;
    RendererContext* renderer_ctx;

    // text rendering
    sgl_pipeline pip_blend;
    FONScontext* fons;
    int font_id;

    // perturbation specific resources
    sg_image orbit_tex;
    sg_view orbit_tex_view;
    sg_sampler orbit_smp;
    int orbit_len;
    int use_perturbation;
    int active_perturbation_last; // cached result from orbit computation step
} GlobalCtx;

static GlobalCtx ctx;

// allocates or resizes persistent capture buffers if window size changes
static int ensure_capture_buffers(void) {
    if (ctx.capture_w != ctx.win_w || ctx.capture_h != ctx.win_h || !ctx.capture_buf) {
        free(ctx.capture_buf);
        ctx.capture_buf = (uint32_t*)malloc((size_t)ctx.win_w * ctx.win_h * sizeof(uint32_t));
        if (!ctx.capture_buf) {
            fprintf(stderr, "error: failed to allocate capture buffers\n");
            ctx.capture_buf = NULL;
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
    char* vs_src = read_shader_file("shaders/desktop_gpu_vs.glsl");
    char* fs_gpu_src = read_shader_file("shaders/desktop_gpu_fs_gpu.glsl");
    const char* vs_ptr = vs_src ? vs_src : dg_vs;
    const char* fs_gpu_ptr = fs_gpu_src ? fs_gpu_src : dg_fs_gpu;

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
            }}});

    if (sg_query_shader_state(shd_gpu) != SG_RESOURCESTATE_VALID) {
        printf("error: failed to compile shaders from files. keeping existing shaders.\n");
        sg_destroy_shader(shd_gpu);
        free(vs_src);
        free(fs_gpu_src);
        return;
    }

    if (ctx.pip_gpu.id) sg_destroy_pipeline(ctx.pip_gpu);
    if (ctx.shd_gpu.id) sg_destroy_shader(ctx.shd_gpu);

    ctx.shd_gpu = shd_gpu;

    ctx.pip_gpu = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = ctx.shd_gpu,
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
        .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
        .index_type = SG_INDEXTYPE_UINT16});

    free(vs_src);
    free(fs_gpu_src);
    printf("shaders loaded/reloaded successfully.\n");
}

static void init(void) {
    sg_setup(&(sg_desc){.environment = sglue_environment()});
    sgl_setup(&(sgl_desc_t){0});
    stm_setup();

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

    app_state_init(&ctx.core, ctx.win_w, ctx.win_h);
    ctx.core.julia_mode = 0;
    ctx.core.base_fractal = 0;

    ctx.high_precision_mode = 1;
    ctx.screenshot_requested = 0;

    float verts[] = {-1, 1, 0, 0, 1, 1, 1, 0, 1, -1, 1, 1, -1, -1, 0, 1};
    ctx.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});
    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    ctx.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.index_buffer = true, .immutable = true}, .data = SG_RANGE(idx)});

    // sokol shader needs something bound to slot 0 even when standard rendering doesn't use it
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

    reload_shaders();

    ctx.pip_blend = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0].blend = {.enabled = true,
                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA}});

    init_fractal_registry();
    ctx.renderer_ctx = init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
    if (!ctx.renderer_ctx) {
        fprintf(stderr, "error: failed to initialize renderer context\n");
        exit(1);
    }
    ctx.core.thread_count = get_actual_thread_count(ctx.renderer_ctx);

    // print controls console guide at startup
    puts("mandelbrot explorer (deep zoom)");
    puts("  left drag   : zoom selection   | right drag  : pan");
    puts("  scroll      : zoom at cursor   | ctrl+z      : undo");
    puts("  up/down     : iterations       | shift+up/dn : x100");
    puts("  p           : cycle palette    | r           : reset");
    puts("  e           : toggle precision (32/64-bit Dekker)");
    puts("  n           : perturbation     | s           : screenshot");
    puts("  h           : toggle help menu | q / esc     : quit");
}

static void frame(void) {
    #define MIN_ORBIT_LEN 20
    uint32_t now = (uint32_t)stm_ms(stm_now());

    // only trigger perturbation at deep zoom levels, float-based rendering is faster when zoomed out
    int can_use_perturbation = ctx.use_perturbation &&
                               (ctx.core.cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD);

    if (!can_use_perturbation) {
        ctx.active_perturbation_last = 0;
    } else if (ctx.core.needs_redraw) {
        precise_float center_re = ctx.core.cam.view.center_re;
        precise_float center_im = ctx.core.cam.view.center_im;
        int max_iters = ctx.core.max_iterations;
        if (max_iters > MAX_ITERATIONS_LIMIT) max_iters = MAX_ITERATIONS_LIMIT;

        // calculate reference orbit on cpu with high precision, then upload coords to gpu texture
        RefOrbit* orb = perturbation_compute(center_re, center_im, max_iters);
        if (orb && orb->len >= MIN_ORBIT_LEN) {
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
            ctx.active_perturbation_last = 0;
        }
        if (orb) perturbation_free(orb);
    }

    sgl_viewport(0, 0, ctx.win_w, ctx.win_h, true);
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float)ctx.win_w, (float)ctx.win_h, 0.0f, -1.0f, 1.0f);

    sg_begin_pass(&(sg_pass){.action = ctx.pass_action, .swapchain = sglue_swapchain()});

    sg_apply_pipeline(ctx.pip_gpu);

    int active_perturbation = ctx.active_perturbation_last;
    if (active_perturbation) {
        ctx.bind.views[0] = ctx.orbit_tex_view;
        ctx.bind.samplers[0] = ctx.orbit_smp;
    } else {
        ctx.bind.views[0] = ctx.dummy_img_view;
        ctx.bind.samplers[0] = ctx.dummy_smp;
    }

    precise_float aspect = (precise_float)ctx.win_w / ctx.win_h;
    precise_float center_re = ctx.core.cam.view.center_re;
    precise_float center_im = ctx.core.cam.view.center_im;

    params_t params = {0};
    // split precise coordinate into high/low float parts for double-single dekker arithmetic in shader
    params.center_hi[0] = (float)center_re;
    params.center_lo[0] = (float)(center_re - (precise_float)params.center_hi[0]);
    params.center_hi[1] = (float)center_im;
    params.center_lo[1] = (float)(center_im - (precise_float)params.center_hi[1]);
    params.zoom = (float)ctx.core.cam.view.zoom;
    params.iters = (float)ctx.core.max_iterations;
    params.aspect = (float)aspect;
    params.fractal_type = 0.0f;
    params.palette = (float)ctx.core.palette_idx;
    params.high_precision = (float)ctx.high_precision_mode;
    params.use_perturbation = (float)active_perturbation;
    params.orbit_len = (float)ctx.orbit_len;
    // keep the sub-float remainder of zoom for dekker d0 calculation in glsl
    params.zoom_lo = (float)(ctx.core.cam.view.zoom - (precise_float)params.zoom);

    sg_apply_uniforms(0, &SG_RANGE(params));
    sg_apply_bindings(&ctx.bind);
    sg_draw(0, 6, 1);

    if (ctx.screenshot_requested) {
        if (ensure_capture_buffers()) {
            glReadPixels(0, 0, ctx.win_w, ctx.win_h, GL_RGBA, GL_UNSIGNED_BYTE, ctx.capture_buf);
            save_screenshot(&ctx.core, ctx.capture_buf, ctx.win_w, ctx.win_h, now, 0, 1);
            ctx.screenshot_requested = 0;
        } else {
            ctx.screenshot_requested = 0;
        }
    }

    if (ctx.core.cam.is_zooming) {
        sgl_load_pipeline(ctx.pip_blend);
        sgl_begin_lines();
        sgl_c4b(255, 255, 0, 255);
        float x0 = (float)ctx.core.cam.zoom_rect.x;
        float y0 = (float)ctx.core.cam.zoom_rect.y;
        float x1 = x0 + (float)ctx.core.cam.zoom_rect.w;
        float y1 = y0 + (float)ctx.core.cam.zoom_rect.h;
        sgl_v2f(x0, y0); sgl_v2f(x1, y0);
        sgl_v2f(x1, y0); sgl_v2f(x1, y1);
        sgl_v2f(x1, y1); sgl_v2f(x0, y1);
        sgl_v2f(x0, y1); sgl_v2f(x0, y0);
        sgl_end();
    }
    
    hud_render_sokol_deep(ctx.fons, ctx.font_id, &ctx.core, ctx.win_w, ctx.win_h,
                          0, 0, 1, ctx.pip_blend, now);

    sgl_draw();

    ctx.core.needs_redraw = 0;
    if (app_state_has_active_notifications(&ctx.core)) {
        ctx.core.needs_redraw = 1;
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
            if (ev->window_width > 0 && ev->window_height > 0) {
                ctx.win_w = ev->framebuffer_width;
                ctx.win_h = ev->framebuffer_height;
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
            case ACTION_QUIT:
                sapp_request_quit();
                break;
            case ACTION_TOGGLE_PERTURBATION:
                ctx.use_perturbation = !ctx.use_perturbation;
                ctx.core.needs_redraw = 1;
                app_state_push_notification(&ctx.core, ctx.use_perturbation ? "Perturbation: Enabled" : "Perturbation: Disabled", now);
                break;
            case ACTION_TOGGLE_PRECISION:
                ctx.high_precision_mode = !ctx.high_precision_mode;
                ctx.core.needs_redraw = 1;
                app_state_push_notification(&ctx.core, ctx.high_precision_mode ? "Precision: 128-bit (Perturb)" : "Precision: 64-bit (Double)", now);
                break;
            case ACTION_RELOAD_SHADERS:
                reload_shaders();
                ctx.core.needs_redraw = 1;
                break;
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
    free(ctx.capture_buf);
    cleanup_renderer(ctx.renderer_ctx);
    cleanup_color_palette();
    sfons_destroy(ctx.fons);
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
                       .window_title = "Mandelbrot Deep Zoom Engine",
                       .high_dpi = false};
}
