/* desktop_cpu_main.c
 *
 * multi-threaded cpu fractal explorer using sdl2.
 * handles input loops, thread scaling, and video rendering.
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
#include "color.h"
#include "config.h"
#include "hud_sdl.h"
#include "ini_config.h"
#include "input_handler.h"
#include "settings_panel_sdl.h"
#include "tour.h"
#include "julia.h"
#include "mandelbrot.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"

// application global context
typedef struct {
    AppCommonState core;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    TTF_Font* font;
    uint32_t* pixels;
    int win_w, win_h;

    int cpu_precision_128;
    int screenshot_requested;
    RendererContext* renderer_ctx;
    SettingsPanelSdl settings;
} AppCtx;

static SDL_Window* g_window = NULL;

// callback to update the sdl window title
static void set_window_title_cb(const char* title) {
    if (g_window) SDL_SetWindowTitle(g_window, title);
}

// internal helpers
static void print_controls(void);

static InputKey map_sdl_key(SDL_Keycode sym) {
    switch (sym) {
        case SDLK_ESCAPE: return KEY_ESCAPE;
        case SDLK_q: return KEY_Q;
        case SDLK_h: case SDLK_F1: return KEY_H;
        case SDLK_z: return KEY_Z;
        case SDLK_r: return KEY_R;
        case SDLK_p: return KEY_P;
        case SDLK_0: return KEY_0;
        case SDLK_1: return KEY_1;
        case SDLK_2: return KEY_2;
        case SDLK_3: return KEY_3;
        case SDLK_4: return KEY_4;
        case SDLK_5: return KEY_5;
        case SDLK_6: return KEY_6;
        case SDLK_7: return KEY_7;
        case SDLK_8: return KEY_8;
        case SDLK_9: return KEY_9;
        case SDLK_UP: return KEY_UP;
        case SDLK_DOWN: return KEY_DOWN;
        case SDLK_n: return KEY_N;
        case SDLK_e: return KEY_E;
        case SDLK_g: return KEY_G;
        case SDLK_j: return KEY_J;
        case SDLK_b: return KEY_B;
        case SDLK_f: return KEY_F;
        case SDLK_s: return KEY_S;
        case SDLK_x: return KEY_X;
        case SDLK_v: return KEY_V;
        case SDLK_m: return KEY_M;
        case SDLK_l: return KEY_L;
        case SDLK_t: return KEY_T;
        case SDLK_LEFTBRACKET: return KEY_LEFT_BRACKET;
        case SDLK_RIGHTBRACKET: return KEY_RIGHT_BRACKET;
        case SDLK_F5: return KEY_F5;
        case SDLK_i: return KEY_I;
        default: return KEY_UNKNOWN;
    }
}

// handles input and updates application state
static void handle_events(AppCtx* ctx) {
    uint32_t now = SDL_GetTicks();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        AppInputEvent ie = {0};
        int handled = 0;

        switch (event.type) {
            case SDL_QUIT:
                ctx->core.running = 0;
                handled = 1;
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
                handled = 1;
                break;

            case SDL_KEYDOWN:
                ie.type = INPUT_KEY_DOWN;
                ie.key = map_sdl_key(event.key.keysym.sym);
                ie.mod_shift = (SDL_GetModState() & KMOD_SHIFT) ? 1 : 0;
                ie.mod_ctrl = (SDL_GetModState() & KMOD_CTRL) ? 1 : 0;
                break;

            case SDL_KEYUP:
                ie.type = INPUT_KEY_UP;
                ie.key = map_sdl_key(event.key.keysym.sym);
                ie.mod_shift = (SDL_GetModState() & KMOD_SHIFT) ? 1 : 0;
                ie.mod_ctrl = (SDL_GetModState() & KMOD_CTRL) ? 1 : 0;
                break;

            case SDL_MOUSEWHEEL:
                ie.type = INPUT_MOUSE_SCROLL;
#if SDL_VERSION_ATLEAST(2, 0, 18)
                ie.scroll_y = event.wheel.preciseY;
#else
                ie.scroll_y = (double)event.wheel.y;
#endif
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ie.scroll_y *= -1.0;
                break;

            case SDL_MOUSEBUTTONDOWN:
                ie.type = INPUT_MOUSE_DOWN;
                ie.mouse_btn = event.button.button;
                ie.mouse_x = event.button.x;
                ie.mouse_y = event.button.y;
                {
                    SdlPanelAction sa = settings_panel_sdl_mouse_down(
                        &ctx->settings, &ctx->core, ie.mouse_x, ie.mouse_y,
                        ctx->win_w, ctx->win_h);
                    if (sa == SDL_PANEL_ACTION_TOGGLE_PRECISION) {
#ifdef USE_SIMD_F128
                        ctx->cpu_precision_128 = !ctx->cpu_precision_128;
                        set_cpu_precision(ctx->renderer_ctx, ctx->cpu_precision_128);
                        ctx->core.needs_redraw = 1;
                        app_state_push_notification(&ctx->core, ctx->cpu_precision_128 ? "Precision: 128-bit (SIMD)" : "Precision: 64-bit (Double)", now);
#endif
                        handled = 1;
                    } else if (sa == SDL_PANEL_ACTION_THREADS_UP || sa == SDL_PANEL_ACTION_THREADS_DOWN) {
                        int threads = get_actual_thread_count(ctx->renderer_ctx);
                        threads += (sa == SDL_PANEL_ACTION_THREADS_UP) ? 1 : -1;
                        set_renderer_thread_count(ctx->renderer_ctx, threads);
                        ctx->core.thread_count = get_actual_thread_count(ctx->renderer_ctx);
                        ctx->core.needs_redraw = 1;
                        char tbuf[64];
                        snprintf(tbuf, sizeof(tbuf), "CPU Threads: %d Cores", ctx->core.thread_count);
                        app_state_push_notification(&ctx->core, tbuf, now);
                        handled = 1;
                    } else if (sa != SDL_PANEL_ACTION_NONE) {
                        handled = 1;
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                ie.type = INPUT_MOUSE_MOVE;
                ie.mouse_x = event.motion.x;
                ie.mouse_y = event.motion.y;
                if (settings_panel_sdl_mouse_move(&ctx->settings, &ctx->core, ie.mouse_x)) {
                    ctx->core.needs_redraw = 1;
                    handled = 1;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                ie.type = INPUT_MOUSE_UP;
                ie.mouse_btn = event.button.button;
                ie.mouse_x = event.button.x;
                ie.mouse_y = event.button.y;
                settings_panel_sdl_mouse_up(&ctx->settings, &ctx->core);
                break;

            default:
                handled = 1; // ignore other events
                break;
        }

        if (!handled) {
            InputAction action = app_handle_input(&ctx->core, &ie, now);
            switch (action) {
                case ACTION_QUIT:
                    ctx->core.running = 0;
                    break;
                case ACTION_TOGGLE_PRECISION:
#ifdef USE_SIMD_F128
                    ctx->cpu_precision_128 = !ctx->cpu_precision_128;
                    set_cpu_precision(ctx->renderer_ctx, ctx->cpu_precision_128);
                    ctx->core.needs_redraw = 1;
                    app_state_push_notification(&ctx->core, ctx->cpu_precision_128 ? "Precision: 128-bit (SIMD)" : "Precision: 64-bit (Double)", now);
#endif
                    break;
                case ACTION_MEGA_SCREENSHOT:
                    if (ctx->core.mega_screenshot_active == 0) {
                        precise_float rmin, rmax, imin, imax;
                        app_state_calculate_boundaries(&ctx->core, ctx->win_w, ctx->win_h, &rmin, &rmax, &imin, &imax);
                        save_mega_screenshot_async(ctx->renderer_ctx, &ctx->core, 8192, 8192, rmin, rmax, imin, imax, ctx->core.max_iterations, ctx->core.palette_idx, ctx->core.julia_mode ? 1 : (ctx->core.base_fractal ? 2 : 0), ctx->core.julia_c);
                        ctx->core.needs_redraw = 1;
                        app_state_push_notification(&ctx->core, "Generating 8K Image...", now);
                    }
                    break;
                case ACTION_TOGGLE_VIDEO:
                    if (is_video_recording()) {
                        stop_video_recording();
                        app_state_push_notification(&ctx->core, "Video Recording Saved!", now);
                    } else {
                        if (start_video_recording(ctx->win_w, ctx->win_h, 60, 1)) {
                            app_state_push_notification(&ctx->core, "Video Recording Started", now);
                        } else {
                            app_state_push_notification(&ctx->core, "Error: ffmpeg not found!", now);
                        }
                    }
                    break;
                case ACTION_RESIZE_THREADS_UP:
                case ACTION_RESIZE_THREADS_DOWN: {
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
                default:
                    if (ie.type == INPUT_KEY_DOWN && ie.key == KEY_S) {
                        ctx->screenshot_requested = 1;
                        ctx->core.needs_redraw = 1;
                    } else if (ie.type == INPUT_KEY_DOWN && ie.key == KEY_I) {
                        ctx->settings.visible = !ctx->settings.visible;
                        app_state_push_notification(&ctx->core,
                            ctx->settings.visible ? "Settings: Open" : "Settings: Closed", now);
                        ctx->core.needs_redraw = 1;
                    }
                    break;
            }
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
                render_julia_threaded(ctx->renderer_ctx, pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax, imax, imin,
                                      ctx->core.julia_c, ctx->core.max_iterations);
            else if (ctx->core.base_fractal)
                render_burning_ship_threaded(ctx->renderer_ctx, pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax,
                                             imax, imin, ctx->core.max_iterations);
            else
                render_mandelbrot_threaded(ctx->renderer_ctx, pixels, pitch, ctx->win_w, ctx->win_h, rmin, rmax, imax,
                                           imin, ctx->core.max_iterations);
            if (is_video_recording()) {
                append_video_frame(pixels, ctx->win_w, ctx->win_h);
            }
            if (ctx->screenshot_requested) {
                save_screenshot(&ctx->core, pixels, ctx->win_w, ctx->win_h, SDL_GetTicks(), 1, 0);
                ctx->screenshot_requested = 0;
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

    if (is_video_recording()) {
        SDL_SetRenderDrawColor(ctx->renderer, 255, 0, 0, 255);
        for (int i = 0; i < 4; i++) {
            SDL_Rect r = {i, i, ctx->win_w - 2 * i, ctx->win_h - 2 * i};
            SDL_RenderDrawRect(ctx->renderer, &r);
        }
    }
}

// draws the debug heads-up display (hud)
static void render_hud(AppCtx* ctx) {
    uint32_t now = SDL_GetTicks();
    hud_render_sdl(ctx->renderer, ctx->font, &ctx->core, ctx->win_w, ctx->win_h,
                   ctx->cpu_precision_128, now);
    settings_panel_sdl_render(&ctx->settings, ctx->renderer, ctx->font, &ctx->core,
                               ctx->win_w, ctx->win_h,
                               ctx->cpu_precision_128, ctx->core.thread_count, now);
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
    ctx.screenshot_requested = 0;

    init_fractal_registry();
    ctx.renderer_ctx = init_renderer(ctx.core.max_iterations, ctx.core.palette_idx);
    if (!ctx.renderer_ctx) {
        fprintf(stderr, "error: failed to initialize renderer context\n");
        if (ctx.font) TTF_CloseFont(ctx.font);
        SDL_DestroyTexture(ctx.texture);
        SDL_DestroyRenderer(ctx.renderer);
        SDL_DestroyWindow(ctx.window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    ctx.core.thread_count = get_actual_thread_count(ctx.renderer_ctx);
    print_controls();

    while (ctx.core.running) {
        // update tour animation state machines
        Uint32 now = SDL_GetTicks();
        app_state_update_tours(&ctx.core, now, set_window_title_cb);

        // force redraw to maintain steady frame pacing during video export, notification overlays, or mega screenshot rendering
        if (is_video_recording() || app_state_has_active_notifications(&ctx.core) || ctx.core.mega_screenshot_active == 1) {
            ctx.core.needs_redraw = 1;
        }

        // check if mega screenshot finished
        if (ctx.core.mega_screenshot_active == 2) {
            app_state_update_or_push_notification(&ctx.core, "Generating 8K", "Mega Screenshot Saved!", now);
            ctx.core.mega_screenshot_active = 0;
        } else if (ctx.core.mega_screenshot_active == 3) {
            app_state_update_or_push_notification(&ctx.core, "Generating 8K", "Error: Mega Screenshot failed!", now);
            ctx.core.mega_screenshot_active = 0;
        }

        handle_events(&ctx);
        render_frame(&ctx);
        render_hud(&ctx);
        SDL_RenderPresent(ctx.renderer);
    }

    if (ctx.font) TTF_CloseFont(ctx.font);
    cleanup_renderer(ctx.renderer_ctx);
    cleanup_color_palette();
    SDL_DestroyTexture(ctx.texture);
    SDL_DestroyRenderer(ctx.renderer);
    SDL_DestroyWindow(ctx.window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

// print controls console guide at startup
static void print_controls(void) {
    puts("mandelbrot explorer");
    puts("  left drag   : zoom selection   | right drag  : pan");
    puts("  scroll      : zoom at cursor   | ctrl+z      : undo");
    puts("  up/down     : iterations       | shift+up/dn : x100");
    puts("  p           : cycle palette    | r           : reset");
    puts("  e           : toggle precision (64/128-bit)");
    puts("  j           : julia mode       | t           : tour");
    puts("  f           : cycle fractals   | s           : screenshot");
    puts("  m           : save bookmark    | l           : load bookmark");
    puts("  x           : mega screenshot  | v           : record video");
    puts("  [ / ]       : scale threads    | h           : toggle help menu");
    puts("  i           : settings panel   | q / esc     : quit");
}
