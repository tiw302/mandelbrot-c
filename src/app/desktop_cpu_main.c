/* desktop_cpu_main.c
 *
 * multi-threaded cpu fractal explorer using sdl2.
 * uses a modular context-based architecture for clean state management.
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

// transient state for julia mode transitions
typedef struct {
    ViewState mandelbrot_view;
    int active;
} JuliaSession;

// application global context
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    TTF_Font* font;
    int win_w, win_h;

    // navigation state
    Camera cam;

    // runtime modes
    TourState m_tour;
    JuliaTourState j_tour;
    int julia_mode, burning_ship_mode;
    complex_t julia_c;
    JuliaSession julia_session;

    // renderer state
    int max_iterations, palette_idx;
    int cpu_precision_128;
    int running, needs_redraw;
    Uint32 render_time_ms;

    int current_bookmark_idx;
} AppCtx;

#define JULIA_ZOOM 4.0

// internal helpers
static void calculate_boundaries(precise_float center_re, precise_float center_im,
                                 precise_float zoom, int width, int height, precise_float* re_min,
                                 precise_float* re_max, precise_float* im_min,
                                 precise_float* im_max);
static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y,
                        SDL_Color color);
static void print_controls(void);

// maps mouse position to complex plane coordinates
static void get_mouse_coord(const AppCtx* ctx, double* re, double* im) {
    precise_float pre, pim;
    camera_screen_to_complex(&ctx->cam, ctx->cam.mouse_x, ctx->cam.mouse_y, &pre, &pim);
    *re = (double)pre;
    *im = (double)pim;
}

// handles keyboard input events
static void handle_keydown(AppCtx* ctx, SDL_Event* event) {
    if (event->key.keysym.sym == SDLK_ESCAPE || event->key.keysym.sym == SDLK_q) {
        ctx->running = 0;
    } else if (event->key.keysym.sym == SDLK_z && (SDL_GetModState() & KMOD_CTRL)) {
        if (camera_pop_history(&ctx->cam)) {
            ctx->needs_redraw = 1;
        }
    } else if (event->key.keysym.sym == SDLK_r) {
        ctx->julia_mode = ctx->julia_session.active = 0;
        ctx->m_tour.phase = TOUR_IDLE;
        ctx->max_iterations = get_config_default_iterations();
        init_renderer(ctx->max_iterations, ctx->palette_idx);
        camera_reset(&ctx->cam);
        SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
        ctx->needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_j) {
        if (!ctx->julia_mode) {
            ctx->julia_session.mandelbrot_view = ctx->cam.view;
            ctx->julia_session.active = 1;
            get_mouse_coord(ctx, &ctx->julia_c.re, &ctx->julia_c.im);
            ctx->cam.view = (ViewState){0.0, 0.0, JULIA_ZOOM};
            ctx->julia_mode = 1;
            SDL_SetWindowTitle(ctx->window, "Julia Explorer");
        } else {
            if (ctx->julia_session.active) ctx->cam.view = ctx->julia_session.mandelbrot_view;
            ctx->julia_mode = 0;
            SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
        }
        ctx->cam.history_count = 0;
        ctx->needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_b) {
        ctx->burning_ship_mode = !ctx->burning_ship_mode;
        ctx->julia_mode = ctx->julia_session.active = 0;
        camera_reset(&ctx->cam);
        SDL_SetWindowTitle(
            ctx->window, ctx->burning_ship_mode ? "Burning Ship Explorer" : "Mandelbrot Explorer");
        ctx->needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_p) {
        ctx->palette_idx = (ctx->palette_idx + 1) % get_palette_count();
        init_renderer(ctx->max_iterations, ctx->palette_idx);
        ctx->needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_e) {
#ifdef USE_SIMD_F128
        ctx->cpu_precision_128 = !ctx->cpu_precision_128;
        set_cpu_precision(ctx->cpu_precision_128);
        ctx->needs_redraw = 1;
#endif
    } else if (event->key.keysym.sym == SDLK_m) {
        Bookmark b = {ctx->cam.view.center_re,
                      ctx->cam.view.center_im,
                      ctx->cam.view.zoom,
                      ctx->max_iterations,
                      ctx->julia_mode ? 1 : (ctx->burning_ship_mode ? 2 : 0),
                      ctx->julia_c};
        save_bookmark(&b);
    } else if (event->key.keysym.sym == SDLK_l) {
        Bookmark b;
        int count = get_bookmark_count();
        if (count > 0) {
            camera_push_history(&ctx->cam);
            ctx->current_bookmark_idx = (ctx->current_bookmark_idx + 1) % count;
            if (load_bookmark(ctx->current_bookmark_idx, &b)) {
                ctx->cam.view = (ViewState){b.center_re, b.center_im, b.zoom};
                ctx->max_iterations = b.max_iterations;
                ctx->julia_mode = (b.fractal_type == 1);
                ctx->burning_ship_mode = (b.fractal_type == 2);
                ctx->julia_c = b.julia_c;
                init_renderer(ctx->max_iterations, ctx->palette_idx);
                ctx->needs_redraw = 1;
            }
        }
    } else if (event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_DOWN) {
        int step = ctx->max_iterations / 10;
        if (step < 10) step = 10;
        if (SDL_GetModState() & KMOD_SHIFT) step *= 10;
        ctx->max_iterations += (event->key.keysym.sym == SDLK_UP) ? step : -step;
        if (ctx->max_iterations < 10) ctx->max_iterations = 10;
        if (ctx->max_iterations > get_config_max_iterations_limit())
            ctx->max_iterations = get_config_max_iterations_limit();
        init_renderer(ctx->max_iterations, ctx->palette_idx);
        ctx->needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_s) {
        uint32_t* ss = malloc((size_t)ctx->win_w * ctx->win_h * 4);
        if (ss) {
            SDL_RenderReadPixels(ctx->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, ss, ctx->win_w * 4);
            save_screenshot(ss, ctx->win_w, ctx->win_h);
            free(ss);
        }
    } else if (event->key.keysym.sym == SDLK_x) {
        precise_float rmin, rmax, imax, imin;
        calculate_boundaries(ctx->cam.view.center_re, ctx->cam.view.center_im, ctx->cam.view.zoom,
                             ctx->win_w, ctx->win_h, &rmin, &rmax, &imin, &imax);
        save_mega_screenshot(8192, 8192, rmin, rmax, imin, imax, ctx->max_iterations,
                             ctx->palette_idx,
                             ctx->julia_mode ? 1 : (ctx->burning_ship_mode ? 2 : 0), ctx->julia_c);
        ctx->needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_v) {
        if (is_video_recording())
            stop_video_recording();
        else
            start_video_recording(ctx->win_w, ctx->win_h, 60);
    } else if (event->key.keysym.sym == SDLK_t) {
        if (ctx->julia_mode) {
            if (ctx->j_tour.phase == JULIA_TOUR_IDLE) {
                start_julia_tour(&ctx->j_tour, &ctx->julia_c, SDL_GetTicks());
                SDL_SetWindowTitle(ctx->window, "Julia Explorer  [Auto-c]");
            } else {
                stop_julia_tour(&ctx->j_tour);
                SDL_SetWindowTitle(ctx->window, "Julia Explorer");
                ctx->needs_redraw = 1;
            }
        } else {
            if (ctx->m_tour.phase == TOUR_IDLE) {
                ctx->julia_mode = ctx->julia_session.active = 0;
                camera_reset(&ctx->cam);
                start_tour(&ctx->m_tour, &ctx->cam.view);
                SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer  [Auto-Zoom]");
            } else {
                stop_tour(&ctx->m_tour);
                camera_reset(&ctx->cam);
                SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
                ctx->needs_redraw = 1;
            }
        }
    } else if (event->key.keysym.sym == SDLK_LEFTBRACKET ||
               event->key.keysym.sym == SDLK_RIGHTBRACKET) {
        int threads = get_actual_thread_count();
        threads += (event->key.keysym.sym == SDLK_RIGHTBRACKET) ? 1 : -1;
        set_renderer_thread_count(threads);
        ctx->needs_redraw = 1;
    }
}

// handles mouse input events
static void handle_mouse(AppCtx* ctx, SDL_Event* event) {
    switch (event->type) {
        case SDL_MOUSEWHEEL: {
#if SDL_VERSION_ATLEAST(2, 0, 18)
            double y_delta = event->wheel.preciseY;
#else
            double y_delta = (double)event->wheel.y;
#endif
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                y_delta *= -1.0;
            }
            int mx = ctx->cam.mouse_x;
            int my = ctx->cam.mouse_y;
#if SDL_VERSION_ATLEAST(2, 0, 26)
            mx = event->wheel.mouseX;
            my = event->wheel.mouseY;
#endif
            camera_handle_wheel(&ctx->cam, y_delta, mx, my);
            ctx->needs_redraw = 1;
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            camera_handle_mouse_down(&ctx->cam, event->button.button, event->button.x,
                                     event->button.y);
            break;

        case SDL_MOUSEMOTION:
            camera_handle_mouse_motion(&ctx->cam, event->motion.x, event->motion.y);
            if (ctx->cam.is_panning) {
                ctx->needs_redraw = 1;
            } else if (ctx->julia_mode && !ctx->cam.is_zooming) {
                get_mouse_coord(ctx, &ctx->julia_c.re, &ctx->julia_c.im);
                ctx->needs_redraw = 1;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (camera_handle_mouse_up(&ctx->cam, event->button.button)) {
                ctx->needs_redraw = 1;
            }
            break;
    }
}

// handles input and updates application state
static void handle_events(AppCtx* ctx) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                ctx->running = 0;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    ctx->win_w = event.window.data1;
                    ctx->win_h = event.window.data2;
                    camera_resize(&ctx->cam, ctx->win_w, ctx->win_h);
                    SDL_DestroyTexture(ctx->texture);
                    ctx->texture =
                        SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, ctx->win_w, ctx->win_h);
                    ctx->needs_redraw = 1;
                }
                break;

            case SDL_KEYDOWN:
                handle_keydown(ctx, &event);
                break;

            case SDL_MOUSEWHEEL:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONUP:
                handle_mouse(ctx, &event);
                break;
        }
    }
}

// renders the fractal and updates visuals
static void render_frame(AppCtx* ctx) {
    if (ctx->needs_redraw && ctx->texture) {
        Uint32 start = SDL_GetTicks();
        Uint32* pixels;
        int pitch;
        if (SDL_LockTexture(ctx->texture, NULL, (void**)&pixels, &pitch) == 0) {
            precise_float rmin, rmax, imax, imin;
            calculate_boundaries(ctx->cam.view.center_re, ctx->cam.view.center_im,
                                 ctx->cam.view.zoom, ctx->win_w, ctx->win_h, &rmin, &rmax, &imin,
                                 &imax);
            if (ctx->julia_mode)
                render_julia_threaded(pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax, imax, imin,
                                      ctx->julia_c, ctx->max_iterations);
            else if (ctx->burning_ship_mode)
                render_burning_ship_threaded(pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax,
                                             imax, imin, ctx->max_iterations);
            else
                render_mandelbrot_threaded(pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax, imax,
                                           imin, ctx->max_iterations);
            if (is_video_recording()) {
                append_video_frame(pixels, ctx->win_w, ctx->win_h);
            }
            SDL_UnlockTexture(ctx->texture);
        }
        ctx->render_time_ms = SDL_GetTicks() - start;
        ctx->needs_redraw = 0;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    if (ctx->texture) SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);

    if (ctx->cam.is_zooming) {
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 0, 255);
        SDL_Rect zoom_rect = {ctx->cam.zoom_rect.x, ctx->cam.zoom_rect.y, ctx->cam.zoom_rect.w,
                              ctx->cam.zoom_rect.h};
        SDL_RenderDrawRect(ctx->renderer, &zoom_rect);
    }
}

// draws the debug heads-up display (hud)
static void render_hud(AppCtx* ctx) {
    if (!DEBUG_INFO || !ctx->font) return;

    char buf[256];
    SDL_Color white = {255, 255, 255, 255};
    int x = 15, y = 12, line_h = FONT_SIZE + 6;

    SDL_Rect bg = {5, 5, 700, 3 * line_h + 20};
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx->renderer, 20, 20, 25, 220);
    SDL_RenderFillRect(ctx->renderer, &bg);

    const char* engine_type = ctx->cpu_precision_128 ? "CPU (128-bit)" : "CPU (64-bit)";
    const char* mode_name =
        ctx->julia_mode ? "Julia" : (ctx->burning_ship_mode ? "Burning Ship" : "Mandelbrot");

    snprintf(buf, sizeof(buf), "[ENGINE] %s | Mode: %s | Threads: %d | Render: %u ms", engine_type,
             mode_name, get_actual_thread_count(), ctx->render_time_ms);
    render_text(ctx->renderer, ctx->font, buf, x, y, white);
    y += line_h;

    if (ctx->julia_mode)
        snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", ctx->julia_c.re, ctx->julia_c.im);
    else
        snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)",
                 (double)ctx->cam.view.center_re, (double)ctx->cam.view.center_im);
    render_text(ctx->renderer, ctx->font, buf, x, y, white);
    y += line_h;

    snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
             (double)ctx->cam.view.zoom, ctx->max_iterations,
             get_palette_name(ctx->palette_idx % get_palette_count()));
    render_text(ctx->renderer, ctx->font, buf, x, y, white);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    srand((unsigned)time(NULL));
#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

    load_config_from_file("settings.txt");
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() == -1) return 1;

    AppCtx ctx = {0};
    ctx.win_w = get_config_window_width();
    ctx.win_h = get_config_window_height();
    ctx.window =
        SDL_CreateWindow("Mandelbrot Explorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         ctx.win_w, ctx.win_h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    const char* font_paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                                FONT_PATH_3,     FONT_PATH_4, NULL};
    for (int i = 0; font_paths[i] && font_paths[i][0]; i++) {
        if ((ctx.font = TTF_OpenFont(font_paths[i], FONT_SIZE))) break;
    }

    ctx.renderer =
        SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    ctx.texture = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, ctx.win_w, ctx.win_h);

    camera_init(&ctx.cam, ctx.win_w, ctx.win_h);
    ctx.julia_c = (complex_t){-0.7, 0.27};
    ctx.max_iterations = get_config_default_iterations();
    ctx.palette_idx = get_config_default_palette();
    ctx.running = 1;
    ctx.needs_redraw = 1;
    ctx.current_bookmark_idx = -1;

    init_fractal_registry();
    init_renderer(ctx.max_iterations, ctx.palette_idx);
    print_controls();

    while (ctx.running) {
        // update tour animation state machines
        Uint32 now = SDL_GetTicks();
        if (ctx.julia_mode) {
            if (ctx.j_tour.phase != JULIA_TOUR_IDLE) {
                update_julia_tour(&ctx.j_tour, &ctx.julia_c, now);
                ctx.needs_redraw = 1;
            }
        } else {
            if (ctx.m_tour.phase != TOUR_IDLE) {
                update_tour(&ctx.m_tour, &ctx.cam.view, now, ctx.burning_ship_mode);
                ctx.needs_redraw = 1;
            }
        }

        // force redraw to maintain steady frame pacing during video export
        if (is_video_recording()) {
            ctx.needs_redraw = 1;
        }

        handle_events(&ctx);
        render_frame(&ctx);
        render_hud(&ctx);
        SDL_RenderPresent(ctx.renderer);
    }

    if (ctx.font) TTF_CloseFont(ctx.font);
    cleanup_renderer();
    cleanup_color_palette();
    SDL_DestroyTexture(ctx.texture);
    SDL_DestroyRenderer(ctx.renderer);
    SDL_DestroyWindow(ctx.window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

static void calculate_boundaries(precise_float center_re, precise_float center_im,
                                 precise_float zoom, int width, int height, precise_float* re_min,
                                 precise_float* re_max, precise_float* im_min,
                                 precise_float* im_max) {
    precise_float aspect = (precise_float)width / height;
    *re_min = center_re - zoom * aspect / 2.0;
    *re_max = center_re + zoom * aspect / 2.0;
    *im_max = center_im + zoom / 2.0;
    *im_min = center_im - zoom / 2.0;
}

static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y,
                        SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surface);
}

static void print_controls(void) {
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
    puts("  [ / ]       : scale threads    | q / esc     : quit");
}
