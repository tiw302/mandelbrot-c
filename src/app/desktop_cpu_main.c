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

// application global context
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    TTF_Font* font;
    int win_w, win_h;

    // common application state
    AppCommonState core;

    int cpu_precision_128;
} AppCtx;

static SDL_Window* g_window = NULL;

// callback to update the sdl window title
static void set_window_title_cb(const char* title) {
    if (g_window) SDL_SetWindowTitle(g_window, title);
}

// internal helpers
static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y,
                        SDL_Color color);
static void print_controls(void);

// handles keyboard input events
static void handle_keydown(AppCtx* ctx, SDL_Event* event) {
    if (event->key.keysym.sym == SDLK_ESCAPE || event->key.keysym.sym == SDLK_q) {
        ctx->core.running = 0;
    } else if (event->key.keysym.sym == SDLK_z && (SDL_GetModState() & KMOD_CTRL)) {
        if (camera_pop_history(&ctx->core.cam)) {
            ctx->core.needs_redraw = 1;
        }
    } else if (event->key.keysym.sym == SDLK_r) {
        app_state_reset(&ctx->core, set_window_title_cb);
    } else if (event->key.keysym.sym == SDLK_j) {
        app_state_toggle_julia(&ctx->core, set_window_title_cb);
    } else if (event->key.keysym.sym == SDLK_b) {
        app_state_toggle_burning_ship(&ctx->core, set_window_title_cb);
    } else if (event->key.keysym.sym == SDLK_p) {
        app_state_cycle_palette(&ctx->core);
    } else if (event->key.keysym.sym == SDLK_e) {
#ifdef USE_SIMD_F128
        ctx->cpu_precision_128 = !ctx->cpu_precision_128;
        set_cpu_precision(ctx->cpu_precision_128);
        ctx->core.needs_redraw = 1;
#endif
    } else if (event->key.keysym.sym == SDLK_m) {
        app_state_save_bookmark(&ctx->core);
    } else if (event->key.keysym.sym == SDLK_l) {
        app_state_load_next_bookmark(&ctx->core);
    } else if (event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_DOWN) {
        int step = ctx->core.max_iterations / 10;
        if (step < 10) step = 10;
        if (SDL_GetModState() & KMOD_SHIFT) step *= 10;
        ctx->core.max_iterations += (event->key.keysym.sym == SDLK_UP) ? step : -step;
        if (ctx->core.max_iterations < 10) ctx->core.max_iterations = 10;
        if (ctx->core.max_iterations > get_config_max_iterations_limit())
            ctx->core.max_iterations = get_config_max_iterations_limit();
        init_renderer(ctx->core.max_iterations, ctx->core.palette_idx);
        ctx->core.needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_s) {
        uint32_t* ss = malloc((size_t)ctx->win_w * ctx->win_h * 4);
        if (ss) {
            SDL_RenderReadPixels(ctx->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, ss, ctx->win_w * 4);
            save_screenshot(ss, ctx->win_w, ctx->win_h);
            free(ss);
        }
    } else if (event->key.keysym.sym == SDLK_x) {
        precise_float rmin, rmax, imax, imin;
        app_state_calculate_boundaries(&ctx->core, ctx->win_w, ctx->win_h, &rmin, &rmax, &imin,
                                       &imax);
        save_mega_screenshot(8192, 8192, rmin, rmax, imin, imax, ctx->core.max_iterations,
                             ctx->core.palette_idx,
                             ctx->core.julia_mode ? 1 : (ctx->core.burning_ship_mode ? 2 : 0),
                             ctx->core.julia_c);
        ctx->core.needs_redraw = 1;
    } else if (event->key.keysym.sym == SDLK_v) {
        if (is_video_recording())
            stop_video_recording();
        else
            start_video_recording(ctx->win_w, ctx->win_h, 60);
    } else if (event->key.keysym.sym == SDLK_t) {
        app_state_toggle_tour(&ctx->core, SDL_GetTicks(), set_window_title_cb);
    } else if (event->key.keysym.sym == SDLK_LEFTBRACKET ||
               event->key.keysym.sym == SDLK_RIGHTBRACKET) {
        int threads = get_actual_thread_count();
        threads += (event->key.keysym.sym == SDLK_RIGHTBRACKET) ? 1 : -1;
        set_renderer_thread_count(threads);
        ctx->core.needs_redraw = 1;
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
            int mx = ctx->core.cam.mouse_x;
            int my = ctx->core.cam.mouse_y;
#if SDL_VERSION_ATLEAST(2, 0, 26)
            mx = event->wheel.mouseX;
            my = event->wheel.mouseY;
#endif
            camera_handle_wheel(&ctx->core.cam, y_delta, mx, my);
            ctx->core.needs_redraw = 1;
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            camera_handle_mouse_down(&ctx->core.cam, event->button.button, event->button.x,
                                     event->button.y);
            break;

        case SDL_MOUSEMOTION:
            camera_handle_mouse_motion(&ctx->core.cam, event->motion.x, event->motion.y);
            if (ctx->core.cam.is_panning) {
                ctx->core.needs_redraw = 1;
            } else if (ctx->core.julia_mode && !ctx->core.cam.is_zooming) {
                app_state_get_mouse_coord(&ctx->core, ctx->core.cam.mouse_x, ctx->core.cam.mouse_y,
                                          &ctx->core.julia_c.re, &ctx->core.julia_c.im);
                ctx->core.needs_redraw = 1;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (camera_handle_mouse_up(&ctx->core.cam, event->button.button)) {
                ctx->core.needs_redraw = 1;
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
                ctx->core.running = 0;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    ctx->win_w = event.window.data1 < 1 ? 1 : event.window.data1;
                    ctx->win_h = event.window.data2 < 1 ? 1 : event.window.data2;
                    camera_resize(&ctx->core.cam, ctx->win_w, ctx->win_h);
                    SDL_DestroyTexture(ctx->texture);
                    ctx->texture =
                        SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, ctx->win_w, ctx->win_h);
                    ctx->core.needs_redraw = 1;
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
    if (ctx->core.needs_redraw && ctx->texture) {
        Uint32 start = SDL_GetTicks();
        Uint32* pixels;
        int pitch;
        if (SDL_LockTexture(ctx->texture, NULL, (void**)&pixels, &pitch) == 0) {
            precise_float rmin, rmax, imax, imin;
            app_state_calculate_boundaries(&ctx->core, ctx->win_w, ctx->win_h, &rmin, &rmax, &imin,
                                           &imax);
            if (ctx->core.julia_mode)
                render_julia_threaded(pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax, imax, imin,
                                      ctx->core.julia_c, ctx->core.max_iterations);
            else if (ctx->core.burning_ship_mode)
                render_burning_ship_threaded(pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax,
                                             imax, imin, ctx->core.max_iterations);
            else
                render_mandelbrot_threaded(pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax, imax,
                                           imin, ctx->core.max_iterations);
            if (is_video_recording()) {
                append_video_frame(pixels, ctx->win_w, ctx->win_h);
            }
            SDL_UnlockTexture(ctx->texture);
        }
        ctx->core.render_time_ms = SDL_GetTicks() - start;
        ctx->core.needs_redraw = 0;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    if (ctx->texture) SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);

    if (ctx->core.cam.is_zooming) {
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 0, 255);
        SDL_Rect zoom_rect = {ctx->core.cam.zoom_rect.x, ctx->core.cam.zoom_rect.y,
                              ctx->core.cam.zoom_rect.w, ctx->core.cam.zoom_rect.h};
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
        ctx->core.julia_mode ? "Julia" : (ctx->core.burning_ship_mode ? "Burning Ship" : "Mandelbrot");

    snprintf(buf, sizeof(buf), "[ENGINE] %s | Mode: %s | Threads: %d | Render: %u ms", engine_type,
             mode_name, get_actual_thread_count(), ctx->core.render_time_ms);
    render_text(ctx->renderer, ctx->font, buf, x, y, white);
    y += line_h;

    if (ctx->core.julia_mode)
        snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", ctx->core.julia_c.re,
                 ctx->core.julia_c.im);
    else
        snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)",
                 (double)ctx->core.cam.view.center_re, (double)ctx->core.cam.view.center_im);
    render_text(ctx->renderer, ctx->font, buf, x, y, white);
    y += line_h;

    snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
             (double)ctx->core.cam.view.zoom, ctx->core.max_iterations,
             get_palette_name(ctx->core.palette_idx % get_palette_count()));
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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "error: sdl_init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "error: ttf_init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    AppCtx ctx = {0};
    ctx.win_w = get_config_window_width();
    ctx.win_h = get_config_window_height();
    ctx.window =
        SDL_CreateWindow("Mandelbrot Explorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         ctx.win_w, ctx.win_h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ctx.window) {
        fprintf(stderr, "error: sdl_createwindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    g_window = ctx.window;

    const char* font_paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                                FONT_PATH_3,     FONT_PATH_4, NULL};
    for (int i = 0; font_paths[i] && font_paths[i][0]; i++) {
        if ((ctx.font = TTF_OpenFont(font_paths[i], FONT_SIZE))) break;
    }

    ctx.renderer =
        SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx.renderer) {
        fprintf(stderr, "error: sdl_createrenderer failed: %s\n", SDL_GetError());
        if (ctx.font) TTF_CloseFont(ctx.font);
        SDL_DestroyWindow(ctx.window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    ctx.texture = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, ctx.win_w, ctx.win_h);
    if (!ctx.texture) {
        fprintf(stderr, "error: sdl_createtexture failed: %s\n", SDL_GetError());
        if (ctx.font) TTF_CloseFont(ctx.font);
        SDL_DestroyRenderer(ctx.renderer);
        SDL_DestroyWindow(ctx.window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    app_state_init(&ctx.core, ctx.win_w, ctx.win_h);
    ctx.cpu_precision_128 = 0;

    init_fractal_registry();
    init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
    print_controls();

    while (ctx.core.running) {
        // update tour animation state machines
        Uint32 now = SDL_GetTicks();
        app_state_update_tours(&ctx.core, now, set_window_title_cb);

        // force redraw to maintain steady frame pacing during video export
        if (is_video_recording()) {
            ctx.core.needs_redraw = 1;
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
