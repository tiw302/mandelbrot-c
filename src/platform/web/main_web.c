#define SOKOL_GLES3
#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_debugtext.h"

#include "config.h"
#include "core_math.h"
#include "renderer_cpu.h"
#include "screenshot.h"
#include "tour.h"

#include <emscripten.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void slog_func(const char* tag, uint32_t log_level, uint32_t log_item_id, const char* message_or_null, uint32_t line_nr, const char* filename_or_null, void* user_data) {
    (void)tag; (void)log_level; (void)log_item_id; (void)line_nr; (void)filename_or_null; (void)user_data;
    if (message_or_null) {
        printf("[sokol][%d] %s\n", log_level, message_or_null);
    }
}

typedef struct {
    ViewState mandelbrot_view;
    int       active;
} JuliaSession;

typedef struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    sg_image img;
    sg_view img_view;
    sg_sampler smp;
    uint32_t *pixels;
    int win_w, win_h;

    ViewState     view;
    ViewState     history[MAX_HISTORY_SIZE];
    int           history_count;

    TourState      m_tour;
    JuliaTourState j_tour;

    int          julia_mode;
    complex_t    julia_c;
    JuliaSession julia_session;

    int      max_iterations;
    int      palette_idx;
    int      needs_redraw;
    int      is_panning, is_zooming;
    int      last_mouse_x, last_mouse_y;
    int      mouse_x, mouse_y;
    
    struct {
        int x, y, w, h;
    } zoom_rect;
    
    uint32_t render_time;
} GlobalCtx;

static GlobalCtx ctx;

// Simple shaders to render a textured quad
static const char* vs_src = 
    "#version 330\n"
    "layout(location=0) in vec2 position;\n"
    "layout(location=1) in vec2 texcoord0;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(position, 0.0, 1.0);\n"
    "  uv = texcoord0;\n"
    "}\n";

static const char* fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    " frag_color = texture(tex, uv);\n"
    "}\n";

// For GLES2/WebGL
static const char* vs_src_gles2 = 
    "attribute vec2 position;\n"
    "attribute vec2 texcoord0;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(position, 0.0, 1.0);\n"
    "  uv = texcoord0;\n"
    "}\n";

static const char* fs_src_gles2 =
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, uv);\n"
    "}\n";

#define JULIA_CENTER_RE   0.0
#define JULIA_CENTER_IM   0.0
#define JULIA_ZOOM        4.0

static void calculate_boundaries(double center_re, double center_im, double zoom,
                                  int width, int height,
                                  double *re_min, double *re_max,
                                  double *im_min, double *im_max) {
    if (height <= 0) height = 1;
    double aspect = (double)width / (double)height;
    *im_min = center_im - zoom / 2.0;
    *im_max = center_im + zoom / 2.0;
    *re_min = center_re - (zoom * aspect) / 2.0;
    *re_max = center_re + (zoom * aspect) / 2.0;
}

static void print_controls(void) {
    puts("Mandelbrot Explorer Controls:");
    puts("  Left Drag       : Zoom into selection");
    puts("  Right Drag      : Pan");
    puts("  Mouse Wheel     : Zoom at cursor");
    puts("  Up / Down       : Adjust iterations (+/- 10, Shift for +/- 100)");
    puts("  P               : Cycle color palettes");
    puts("  Ctrl+Z          : Undo zoom");
    puts("  R               : Reset view / iterations");
    puts("  J               : Toggle Julia mode");
    puts("  S               : Save screenshot");
    puts("  T (Mandelbrot)  : Toggle auto-zoom tour");
    puts("  T (Julia)       : Toggle auto-c tour (animates c parameter)");
    puts("  Q / ESC         : Quit");
}

static void init(void) {
    print_controls();
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func
    });
    stm_setup();
    sdtx_setup(&(sdtx_desc_t){
        .fonts[0] = sdtx_font_c64(),
        .logger.func = slog_func
    });

    ctx.win_w = sapp_width();
    ctx.win_h = sapp_height();
    if (ctx.win_w <= 0) ctx.win_w = 1024;
    if (ctx.win_h <= 0) ctx.win_h = 768;

    ctx.pixels = (uint32_t*) malloc((size_t)ctx.win_w * ctx.win_h * sizeof(uint32_t));

    float vertices[] = {
        // pos      uv
        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 1.0f
    };
    ctx.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "quad-vertices"
    });

    uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
    ctx.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .usage.immutable = true,
        .data = SG_RANGE(indices),
        .label = "quad-indices"
    });

    ctx.img = sg_make_image(&(sg_image_desc){
        .width = ctx.win_w,
        .height = ctx.win_h,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.dynamic_update = true,
        .label = "mandelbrot-image"
    });
    
    ctx.img_view = sg_make_view(&(sg_view_desc){
        .texture.image = ctx.img,
        .label = "mandelbrot-view"
    });
    
    ctx.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .label = "mandelbrot-sampler"
    });
    
    ctx.bind.views[0] = ctx.img_view;
    ctx.bind.samplers[0] = ctx.smp;

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "texcoord0"
        },
        .vertex_func.source = vs_src,
        .fragment_func.source = fs_src,
        .views[0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .image_type = SG_IMAGETYPE_2D },
        .samplers[0] = { .stage = SG_SHADERSTAGE_FRAGMENT },
        .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0, .glsl_name = "tex" },
        .label = "mandelbrot-shader"
    });

    ctx.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT2,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .label = "mandelbrot-pipeline"
    });

    ctx.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.m_tour = (TourState){TOUR_IDLE, 0,0,0, 0,0,0, 0, -1};
    ctx.j_tour = (JuliaTourState){JULIA_TOUR_IDLE, 0,0, 0,0, 0, -1};
    ctx.julia_c = (complex_t){-0.7, 0.27};
    ctx.max_iterations = DEFAULT_ITERATIONS;
    ctx.palette_idx = 0;
    ctx.needs_redraw = 1;

    init_renderer(ctx.max_iterations, ctx.palette_idx);
}

static void frame(void) {
    uint32_t now = (uint32_t)stm_ms(stm_now());

    if (ctx.m_tour.phase != TOUR_IDLE) {
        update_tour(&ctx.m_tour, &ctx.view, now);
        ctx.needs_redraw = 1;
    }
    if (ctx.j_tour.phase != JULIA_TOUR_IDLE) {
        update_julia_tour(&ctx.j_tour, &ctx.julia_c, now);
        ctx.needs_redraw = 1;
    }

    if (ctx.needs_redraw) {
        uint64_t start = stm_now();
        double re_min, re_max, im_min, im_max;
        calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                              ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
        
        if (ctx.julia_mode)
            render_julia_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h,
                                  re_min, re_max, im_min, im_max, ctx.julia_c, ctx.max_iterations);
        else
            render_mandelbrot_threaded(ctx.pixels, ctx.win_w * 4, ctx.win_w, ctx.win_h,
                                       re_min, re_max, im_min, im_max, ctx.max_iterations);
        
        sg_update_image(ctx.img, &(sg_image_data){
            .mip_levels[0] = { .ptr = ctx.pixels, .size = (size_t)ctx.win_w * ctx.win_h * 4 }
        });
        
        ctx.render_time = (uint32_t)stm_ms(stm_since(start));
        ctx.needs_redraw = 0;
    }

    if (ctx.is_zooming) {
        int x1 = ctx.zoom_rect.x, y1 = ctx.zoom_rect.y;
        int x2 = x1 + ctx.zoom_rect.w, y2 = y1 + ctx.zoom_rect.h;
        if (x1 > x2) { int t=x1; x1=x2; x2=t; }
        if (y1 > y2) { int t=y1; y1=y2; y2=t; }
        uint32_t yellow = 0xFF00FFFF; // BGRA yellow
        for (int x=x1; x<=x2; x++) {
            if (x>=0 && x<ctx.win_w) {
                if (y1>=0 && y1<ctx.win_h) ctx.pixels[y1*ctx.win_w+x] = yellow;
                if (y2>=0 && y2<ctx.win_h) ctx.pixels[y2*ctx.win_w+x] = yellow;
            }
        }
        for (int y=y1; y<=y2; y++) {
            if (y>=0 && y<ctx.win_h) {
                if (x1>=0 && x1<ctx.win_w) ctx.pixels[y*ctx.win_w+x1] = yellow;
                if (x2>=0 && x2<ctx.win_w) ctx.pixels[y*ctx.win_w+x2] = yellow;
            }
        }
        sg_update_image(ctx.img, &(sg_image_data){
            .mip_levels[0] = { .ptr = ctx.pixels, .size = (size_t)ctx.win_w * ctx.win_h * 4 }
        });
    }

    sg_begin_pass(&(sg_pass){ .action = ctx.pass_action, .swapchain = sglue_swapchain() });
    sg_apply_pipeline(ctx.pip);
    sg_apply_bindings(&ctx.bind);
    sg_draw(0, 6, 1);

    // Debug text
    if (DEBUG_INFO) {
        sdtx_canvas(sapp_widthf() * 0.5f, sapp_heightf() * 0.5f);
        sdtx_origin(1.0f, 1.0f);
        sdtx_color3b(255, 255, 255);
        sdtx_printf("%s | Render: %u ms | Threads: %d\n", 
                    ctx.julia_mode ? "JULIA" : "MANDELBROT", ctx.render_time, get_actual_thread_count());
        sdtx_printf("Center: (%.12f, %.12f)\n", ctx.view.center_re, ctx.view.center_im);
        sdtx_printf("Zoom: %.6g | Iterations: %d | Palette: %s\n",
                    ctx.view.zoom, ctx.max_iterations, PALETTE_NAMES[ctx.palette_idx]);
        if (ctx.julia_mode) {
            sdtx_printf("c = (%.6f, %.6f)\n", ctx.julia_c.re, ctx.julia_c.im);
        }
        if (ctx.m_tour.phase != TOUR_IDLE) {
            sdtx_printf("Auto-Zoom [%s] target #%d\n", get_tour_phase_name(ctx.m_tour.phase), get_tour_target_idx(&ctx.m_tour) + 1);
        }
        if (ctx.j_tour.phase != JULIA_TOUR_IDLE) {
            sdtx_printf("Auto-c [%s] #%d (%.4f, %.4f)\n", ctx.j_tour.phase == JULIA_TOUR_MOVING ? "moving" : "dwelling", get_julia_tour_target_idx(&ctx.j_tour) + 1, ctx.julia_c.re, ctx.julia_c.im);
        }
        sdtx_draw();
    }

    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (ctx.m_tour.phase != TOUR_IDLE) {
                ctx.m_tour.phase = TOUR_IDLE;
            }
            if (ctx.j_tour.phase != JULIA_TOUR_IDLE) {
                ctx.j_tour.phase = JULIA_TOUR_IDLE;
            }
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
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
                ctx.is_panning = 0;
            } else if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                if (ctx.is_zooming) {
                    ctx.needs_redraw = 1;
                    if (ctx.zoom_rect.w != 0 && ctx.zoom_rect.h != 0) {
                        if (ctx.history_count < MAX_HISTORY_SIZE)
                            ctx.history[ctx.history_count++] = ctx.view;
                        double re_min, re_max, im_min, im_max;
                        calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                                             ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
                        double re_pp = (re_max - re_min) / ctx.win_w;
                        double im_pp = (im_max - im_min) / ctx.win_h;
                        ctx.view.center_re = re_min + (ctx.zoom_rect.x + (double)ctx.zoom_rect.w / 2.0) * re_pp;
                        ctx.view.center_im = im_min + (ctx.zoom_rect.y + (double)ctx.zoom_rect.h / 2.0) * im_pp;
                        ctx.view.zoom = fmax(fabs((double)ctx.zoom_rect.w) * re_pp,
                                              fabs((double)ctx.zoom_rect.h) * im_pp);
                    }
                }
                ctx.is_zooming = 0;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            ctx.mouse_x = (int)ev->mouse_x;
            ctx.mouse_y = (int)ev->mouse_y;
            if (ctx.is_panning) {
                double re_min, re_max, im_min, im_max;
                calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                                     ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
                ctx.view.center_re -= (ev->mouse_x - ctx.last_mouse_x) * (re_max - re_min) / ctx.win_w;
                ctx.view.center_im -= (ev->mouse_y - ctx.last_mouse_y) * (im_max - im_min) / ctx.win_h;
                ctx.last_mouse_x = (int)ev->mouse_x;
                ctx.last_mouse_y = (int)ev->mouse_y;
                ctx.needs_redraw = 1;
            } else if (ctx.is_zooming) {
                ctx.zoom_rect.w = (int)ev->mouse_x - ctx.zoom_rect.x;
                ctx.zoom_rect.h = (int)ev->mouse_y - ctx.zoom_rect.y;
                ctx.needs_redraw = 1;
            } else if (ctx.julia_mode && ctx.j_tour.phase == JULIA_TOUR_IDLE) {
                double re_min, re_max, im_min, im_max;
                calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                                     ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
                ctx.julia_c.re = re_min + (double)ctx.mouse_x * (re_max - re_min) / ctx.win_w;
                ctx.julia_c.im = im_min + (double)ctx.mouse_y * (im_max - im_min) / ctx.win_h;
                ctx.needs_redraw = 1;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            {
                double zoom_factor = (ev->scroll_y > 0) ? 0.9 : 1.1;
                if (ctx.history_count < MAX_HISTORY_SIZE)
                    ctx.history[ctx.history_count++] = ctx.view;
                double re_min, re_max, im_min, im_max;
                calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                                     ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
                double mouse_re = re_min + (double)ctx.mouse_x * (re_max - re_min) / ctx.win_w;
                double mouse_im = im_min + (double)ctx.mouse_y * (im_max - im_min) / ctx.win_h;
                ctx.view.zoom *= zoom_factor;
                ctx.view.center_re = mouse_re + (ctx.view.center_re - mouse_re) * zoom_factor;
                ctx.view.center_im = mouse_im + (ctx.view.center_im - mouse_im) * zoom_factor;
                ctx.needs_redraw = 1;
            }
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (ev->key_code == SAPP_KEYCODE_R) {
                ctx.julia_mode = 0;
                ctx.julia_session.active = 0;
                ctx.m_tour.phase = TOUR_IDLE;
                ctx.j_tour.phase = JULIA_TOUR_IDLE;
                ctx.max_iterations = DEFAULT_ITERATIONS;
                ctx.palette_idx = 0;
                init_renderer(ctx.max_iterations, ctx.palette_idx);
                ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                ctx.history_count = 0;
                ctx.needs_redraw = 1;
            } else if (ev->key_code == SAPP_KEYCODE_J) {
                ctx.m_tour.phase = TOUR_IDLE;
                ctx.j_tour.phase = JULIA_TOUR_IDLE;
                if (!ctx.julia_mode) {
                    ctx.julia_session.mandelbrot_view = ctx.view;
                    ctx.julia_session.active = 1;
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                                         ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
                    ctx.julia_c.re = re_min + (double)ctx.mouse_x * (re_max - re_min) / ctx.win_w;
                    ctx.julia_c.im = im_min + (double)ctx.mouse_y * (im_max - im_min) / ctx.win_h;
                    ctx.view = (ViewState){JULIA_CENTER_RE, JULIA_CENTER_IM, JULIA_ZOOM};
                    ctx.julia_mode = 1;
                    ctx.history_count = 0;
                } else {
                    if (ctx.julia_session.active) ctx.view = ctx.julia_session.mandelbrot_view;
                    ctx.julia_mode = 0;
                    ctx.history_count = 0;
                }
                ctx.needs_redraw = 1;
            } else if (ev->key_code == SAPP_KEYCODE_UP) {
                int step = (ev->modifiers & SAPP_MODIFIER_SHIFT) ? 100 : 10;
                if (ctx.max_iterations + step <= MAX_ITERATIONS_LIMIT) {
                    ctx.max_iterations += step;
                    init_renderer(ctx.max_iterations, ctx.palette_idx);
                    ctx.needs_redraw = 1;
                }
            } else if (ev->key_code == SAPP_KEYCODE_DOWN) {
                int step = (ev->modifiers & SAPP_MODIFIER_SHIFT) ? 100 : 10;
                if (ctx.max_iterations - step >= 10) {
                    ctx.max_iterations -= step;
                    init_renderer(ctx.max_iterations, ctx.palette_idx);
                    ctx.needs_redraw = 1;
                }
            } else if (ev->key_code == SAPP_KEYCODE_P) {
                ctx.palette_idx = (ctx.palette_idx + 1) % PALETTE_COUNT;
                init_renderer(ctx.max_iterations, ctx.palette_idx);
                ctx.needs_redraw = 1;
            } else if (ev->key_code == SAPP_KEYCODE_T) {
                uint32_t now = (uint32_t)stm_ms(stm_now());
                if (ctx.julia_mode) {
                    if (ctx.j_tour.phase == JULIA_TOUR_IDLE) {
                        ctx.j_tour.from_re = ctx.julia_c.re;
                        ctx.j_tour.from_im = ctx.julia_c.im;
                        ctx.j_tour.phase = JULIA_TOUR_DWELLING;
                        ctx.j_tour.phase_start = now;
                    } else {
                        ctx.j_tour.phase = JULIA_TOUR_IDLE;
                        ctx.needs_redraw = 1;
                    }
                } else {
                    if (ctx.m_tour.phase == TOUR_IDLE) {
                        ctx.julia_mode = 0;
                        ctx.julia_session.active = 0;
                        ctx.history_count = 0;
                        ctx.m_tour.home_re = INITIAL_CENTER_RE;
                        ctx.m_tour.home_im = INITIAL_CENTER_IM;
                        ctx.m_tour.home_zoom = INITIAL_ZOOM;
                        ctx.m_tour.deep_zoom = INITIAL_ZOOM / 6000.0;
                        ctx.view.center_re = ctx.m_tour.home_re;
                        ctx.view.center_im = ctx.m_tour.home_im;
                        ctx.view.zoom = ctx.m_tour.home_zoom;
                        ctx.m_tour.phase = TOUR_ZOOMING_OUT;
                        ctx.m_tour.phase_start = now - 10000;
                    } else {
                        ctx.m_tour.phase = TOUR_IDLE;
                        ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                        ctx.history_count = 0;
                        ctx.needs_redraw = 1;
                    }
                }
            } else if (ev->key_code == SAPP_KEYCODE_Z && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                if (ctx.history_count > 0)
                    ctx.view = ctx.history[--ctx.history_count];
                ctx.needs_redraw = 1;
            }
            break;
        case SAPP_EVENTTYPE_RESIZED:
            ctx.win_w = ev->window_width;
            ctx.win_h = ev->window_height;
            if (ctx.win_w < 1) ctx.win_w = 1;
            if (ctx.win_h < 1) ctx.win_h = 1;
            free(ctx.pixels);
            ctx.pixels = (uint32_t*) malloc((size_t)ctx.win_w * ctx.win_h * sizeof(uint32_t));
            sg_destroy_image(ctx.img);
            sg_destroy_view(ctx.img_view);
            ctx.img = sg_make_image(&(sg_image_desc){
                .width = ctx.win_w,
                .height = ctx.win_h,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .usage.dynamic_update = true
            });
            ctx.img_view = sg_make_view(&(sg_view_desc){
                .texture.image = ctx.img
            });
            ctx.bind.views[0] = ctx.img_view;
            ctx.needs_redraw = 1;
            break;
        default:
            break;
    }
}

static void cleanup(void) {
    free(ctx.pixels);
    cleanup_renderer();
    sdtx_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 1024,
        .height = 768,
        .window_title = "Mandelbrot Sokol",
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}

// --- EMSCRIPTEN INTEROP ---

EMSCRIPTEN_KEEPALIVE
void wasm_reset_view() {
    ctx.julia_mode = 0;
    ctx.julia_session.active = 0;
    ctx.m_tour.phase = TOUR_IDLE;
    ctx.j_tour.phase = JULIA_TOUR_IDLE;
    ctx.max_iterations = DEFAULT_ITERATIONS;
    ctx.palette_idx = 0;
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    ctx.view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx.history_count = 0;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_next_palette() {
    ctx.palette_idx = (ctx.palette_idx + 1) % PALETTE_COUNT;
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia() {
    if (!ctx.julia_mode) {
        ctx.julia_session.mandelbrot_view = ctx.view;
        ctx.julia_session.active = 1;
        ctx.view = (ViewState){JULIA_CENTER_RE, JULIA_CENTER_IM, JULIA_ZOOM};
        ctx.julia_mode = 1;
        ctx.history_count = 0;
    } else {
        if (ctx.julia_session.active) ctx.view = ctx.julia_session.mandelbrot_view;
        ctx.julia_mode = 0;
        ctx.history_count = 0;
    }
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_adjust_iterations(int delta) {
    if (ctx.max_iterations + delta >= 10 && ctx.max_iterations + delta <= MAX_ITERATIONS_LIMIT) {
        ctx.max_iterations += delta;
        init_renderer(ctx.max_iterations, ctx.palette_idx);
        ctx.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_resolution(int w, int h) {
    if (w < 1 || h < 1) return;
    ctx.win_w = w;
    ctx.win_h = h;
    free(ctx.pixels);
    ctx.pixels = (uint32_t*) malloc((size_t)ctx.win_w * ctx.win_h * sizeof(uint32_t));
    sg_destroy_image(ctx.img);
    sg_destroy_view(ctx.img_view);
    ctx.img = sg_make_image(&(sg_image_desc){
        .width = ctx.win_w,
        .height = ctx.win_h,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.dynamic_update = true
    });
    ctx.img_view = sg_make_view(&(sg_view_desc){
        .texture.image = ctx.img
    });
    ctx.bind.views[0] = ctx.img_view;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_zoom_at(int px, int py, double factor) {
    if (ctx.history_count < MAX_HISTORY_SIZE)
        ctx.history[ctx.history_count++] = ctx.view;
    double re_min, re_max, im_min, im_max;
    calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                         ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
    double mouse_re = re_min + (double)px * (re_max - re_min) / ctx.win_w;
    double mouse_im = im_min + (double)py * (im_max - im_min) / ctx.win_h;
    ctx.view.zoom *= factor;
    ctx.view.center_re = mouse_re + (ctx.view.center_re - mouse_re) * factor;
    ctx.view.center_im = mouse_im + (ctx.view.center_im - mouse_im) * factor;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_undo_zoom() {
    if (ctx.history_count > 0) {
        ctx.view = ctx.history[--ctx.history_count];
        ctx.needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_pan(int dx, int dy) {
    double re_min, re_max, im_min, im_max;
    calculate_boundaries(ctx.view.center_re, ctx.view.center_im, ctx.view.zoom,
                         ctx.win_w, ctx.win_h, &re_min, &re_max, &im_min, &im_max);
    ctx.view.center_re -= (double)dx * (re_max - re_min) / ctx.win_w;
    ctx.view.center_im -= (double)dy * (im_max - im_min) / ctx.win_h;
    ctx.needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_cancel_zoom() {
    ctx.is_zooming = 0;
    ctx.is_panning = 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_tour() {
    uint32_t now = (uint32_t)stm_ms(stm_now());
    if (ctx.julia_mode) {
        if (ctx.j_tour.phase == JULIA_TOUR_IDLE) {
            ctx.j_tour.from_re = ctx.julia_c.re;
            ctx.j_tour.from_im = ctx.julia_c.im;
            ctx.j_tour.phase = JULIA_TOUR_DWELLING;
            ctx.j_tour.phase_start = now;
        } else {
            ctx.j_tour.phase = JULIA_TOUR_IDLE;
        }
    } else {
        if (ctx.m_tour.phase == TOUR_IDLE) {
            ctx.m_tour.home_re = INITIAL_CENTER_RE;
            ctx.m_tour.home_im = INITIAL_CENTER_IM;
            ctx.m_tour.home_zoom = INITIAL_ZOOM;
            ctx.m_tour.deep_zoom = INITIAL_ZOOM / 6000.0;
            ctx.m_tour.phase = TOUR_ZOOMING_OUT;
            ctx.m_tour.phase_start = now - 10000;
        } else {
            ctx.m_tour.phase = TOUR_IDLE;
        }
    }
    ctx.needs_redraw = 1;
}
