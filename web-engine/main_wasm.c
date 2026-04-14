#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include "../include/config.h"
#include "../core/mandelbrot.h"
#include "../core/julia.h"
#include "../core/color.h"
#include "renderer_wasm.h"

typedef struct {
    ViewState mandelbrot_view;
    int       active;
} JuliaSession;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    int           win_w, win_h;

    ViewState     view;
    ViewState     history[MAX_HISTORY_SIZE];
    int           history_count;

    int          julia_mode;
    complex_t    julia_c;
    JuliaSession julia_session;

    int      max_iterations;
    int      palette_idx;
    int      needs_redraw;
    int      is_panning, is_zooming;
    int      last_mouse_x, last_mouse_y;
    int      mouse_x, mouse_y;
    SDL_Rect zoom_rect;
    Uint32   render_time;
} GlobalCtx;

static GlobalCtx *g_ctx = NULL;


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

void main_loop_iteration(void *arg) {
    GlobalCtx *ctx = (GlobalCtx *)arg;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                ctx->win_w = event.window.data1;
                ctx->win_h = event.window.data2;
                if (ctx->win_w < 1) ctx->win_w = 1;
                if (ctx->win_h < 1) ctx->win_h = 1;

                SDL_DestroyTexture(ctx->texture);
                ctx->texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, ctx->win_w, ctx->win_h);
                ctx->needs_redraw = 1;
            }
            break;

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_z && (SDL_GetModState() & KMOD_CTRL)) {
                if (ctx->history_count > 0)
                    ctx->view = ctx->history[--ctx->history_count];
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_r) {
                ctx->julia_mode = 0;
                ctx->julia_session.active = 0;
                ctx->max_iterations = DEFAULT_ITERATIONS;
                ctx->palette_idx = 0;
                init_color_palette(ctx->max_iterations, ctx->palette_idx);
                ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                ctx->history_count = 0;
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_j) {
                if (!ctx->julia_mode) {
                    ctx->julia_session.mandelbrot_view = ctx->view;
                    ctx->julia_session.active = 1;
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                                         ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
                    ctx->julia_c.re = re_min + (double)ctx->mouse_x * (re_max - re_min) / ctx->win_w;
                    ctx->julia_c.im = im_min + (double)ctx->mouse_y * (im_max - im_min) / ctx->win_h;
                    ctx->view = (ViewState){JULIA_CENTER_RE, JULIA_CENTER_IM, JULIA_ZOOM};
                    ctx->julia_mode = 1;
                    ctx->history_count = 0;
                } else {
                    if (ctx->julia_session.active) ctx->view = ctx->julia_session.mandelbrot_view;
                    ctx->julia_mode = 0;
                    ctx->history_count = 0;
                }
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_UP) {
                int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                if (ctx->max_iterations + step <= MAX_ITERATIONS_LIMIT) {
                    ctx->max_iterations += step;
                    init_color_palette(ctx->max_iterations, ctx->palette_idx);
                    ctx->needs_redraw = 1;
                }
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                if (ctx->max_iterations - step >= 10) {
                    ctx->max_iterations -= step;
                    init_color_palette(ctx->max_iterations, ctx->palette_idx);
                    ctx->needs_redraw = 1;
                }
            } else if (event.key.keysym.sym == SDLK_p) {
                ctx->palette_idx = (ctx->palette_idx + 1) % PALETTE_COUNT;
                init_color_palette(ctx->max_iterations, ctx->palette_idx);
                ctx->needs_redraw = 1;
            }
            break;

        case SDL_MOUSEWHEEL:
            {
                double zoom_factor = (event.wheel.y > 0) ? 0.9 : 1.1;
                if (event.wheel.y == 0) break;
                if (ctx->history_count < MAX_HISTORY_SIZE)
                    ctx->history[ctx->history_count++] = ctx->view;
                double re_min, re_max, im_min, im_max;
                calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                                     ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
                double mouse_re = re_min + (double)ctx->mouse_x * (re_max - re_min) / ctx->win_w;
                double mouse_im = im_min + (double)ctx->mouse_y * (im_max - im_min) / ctx->win_h;
                ctx->view.zoom *= zoom_factor;
                ctx->view.center_re = mouse_re + (ctx->view.center_re - mouse_re) * zoom_factor;
                ctx->view.center_im = mouse_im + (ctx->view.center_im - mouse_im) * zoom_factor;
                ctx->needs_redraw = 1;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_RIGHT) {
                ctx->is_panning = 1;
                ctx->last_mouse_x = event.button.x;
                ctx->last_mouse_y = event.button.y;
            } else if (event.button.button == SDL_BUTTON_LEFT) {
                ctx->is_zooming = 1;
                ctx->zoom_rect = (SDL_Rect){event.button.x, event.button.y, 0, 0};
            }
            break;

        case SDL_MOUSEMOTION:
            ctx->mouse_x = event.motion.x;
            ctx->mouse_y = event.motion.y;
            if (ctx->is_panning) {
                double re_min, re_max, im_min, im_max;
                calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                                     ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
                ctx->view.center_re -= (event.motion.x - ctx->last_mouse_x) * (re_max - re_min) / ctx->win_w;
                ctx->view.center_im -= (event.motion.y - ctx->last_mouse_y) * (im_max - im_min) / ctx->win_h;
                ctx->last_mouse_x = event.motion.x;
                ctx->last_mouse_y = event.motion.y;
                ctx->needs_redraw = 1;
            } else if (ctx->is_zooming) {
                ctx->zoom_rect.w = event.motion.x - ctx->zoom_rect.x;
                ctx->zoom_rect.h = event.motion.y - ctx->zoom_rect.y;
            } else if (ctx->julia_mode) {
                double re_min, re_max, im_min, im_max;
                calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                                     ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
                ctx->julia_c.re = re_min + (double)ctx->mouse_x * (re_max - re_min) / ctx->win_w;
                ctx->julia_c.im = im_min + (double)ctx->mouse_y * (im_max - im_min) / ctx->win_h;
                ctx->needs_redraw = 1;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_RIGHT) {
                ctx->is_panning = 0;
            } else if (event.button.button == SDL_BUTTON_LEFT) {
                if (ctx->is_zooming && ctx->zoom_rect.w != 0 && ctx->zoom_rect.h != 0) {
                    if (ctx->history_count < MAX_HISTORY_SIZE)
                        ctx->history[ctx->history_count++] = ctx->view;
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                                         ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
                    double re_pp = (re_max - re_min) / ctx->win_w;
                    double im_pp = (im_max - im_min) / ctx->win_h;
                    ctx->view.center_re = re_min + (ctx->zoom_rect.x + ctx->zoom_rect.w / 2.0) * re_pp;
                    ctx->view.center_im = im_min + (ctx->zoom_rect.y + ctx->zoom_rect.h / 2.0) * im_pp;
                    ctx->view.zoom = fmax(fabs((double)ctx->zoom_rect.w) * re_pp,
                                          fabs((double)ctx->zoom_rect.h) * im_pp);
                    ctx->needs_redraw = 1;
                }
                ctx->is_zooming = 0;
            }
            break;
        }
    }

    if (ctx->needs_redraw) {
        Uint32 *pixels; int pitch;
        SDL_LockTexture(ctx->texture, NULL, (void **)&pixels, &pitch);
        double re_min, re_max, im_min, im_max;
        calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                              ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
        if (ctx->julia_mode) {
            render_julia_wasm(pixels, pitch, ctx->win_w, ctx->win_h,
                              re_min, re_max, im_min, im_max, ctx->julia_c, ctx->max_iterations);
        } else {
            render_mandelbrot_wasm(pixels, pitch, ctx->win_w, ctx->win_h,
                                   re_min, re_max, im_min, im_max, ctx->max_iterations);
        }
        SDL_UnlockTexture(ctx->texture);
        ctx->needs_redraw = 0;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);

    if (ctx->is_zooming) {
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 0, 255);
        SDL_RenderDrawRect(ctx->renderer, &ctx->zoom_rect);
    }

    SDL_RenderPresent(ctx->renderer);
}

int main() {
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    GlobalCtx *ctx = calloc(1, sizeof(GlobalCtx));
    if (!ctx) { SDL_Quit(); return 1; }
    g_ctx = ctx;


    ctx->win_w = WINDOW_WIDTH;
    ctx->win_h = WINDOW_HEIGHT;

    ctx->window = SDL_CreateWindow(
        "Mandelbrot Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ctx->win_w, ctx->win_h, SDL_WINDOW_SHOWN);
    if (!ctx->window) {
        free(ctx); SDL_Quit(); return 1;
    }

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED);
    if (!ctx->renderer) {
        SDL_DestroyWindow(ctx->window);
        free(ctx); SDL_Quit(); return 1;
    }

    ctx->texture = SDL_CreateTexture(
        ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, ctx->win_w, ctx->win_h);
    if (!ctx->texture) {
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        free(ctx); SDL_Quit(); return 1;
    }

    ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx->julia_c = (complex_t){-0.7, 0.27};
    ctx->max_iterations = DEFAULT_ITERATIONS;
    ctx->needs_redraw = 1;

    init_color_palette(ctx->max_iterations, ctx->palette_idx);

    /* WebAssembly specific render loop */
    emscripten_set_main_loop_arg(main_loop_iteration, ctx, -1, 1);

    SDL_DestroyTexture(ctx->texture);
    SDL_DestroyRenderer(ctx->renderer);
    SDL_DestroyWindow(ctx->window);
    free(ctx);
    SDL_Quit();
    return 0;
}
EMSCRIPTEN_KEEPALIVE
void wasm_reset_view() {
    if (!g_ctx) return;
    g_ctx->julia_mode = 0;
    g_ctx->julia_session.active = 0;
    g_ctx->max_iterations = DEFAULT_ITERATIONS;
    g_ctx->palette_idx = 0;
    init_color_palette(g_ctx->max_iterations, g_ctx->palette_idx);
    g_ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    g_ctx->history_count = 0;
    g_ctx->needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_next_palette() {
    if (!g_ctx) return;
    g_ctx->palette_idx = (g_ctx->palette_idx + 1) % PALETTE_COUNT;
    init_color_palette(g_ctx->max_iterations, g_ctx->palette_idx);
    g_ctx->needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_toggle_julia() {
    if (!g_ctx) return;
    if (!g_ctx->julia_mode) {
        g_ctx->julia_session.mandelbrot_view = g_ctx->view;
        g_ctx->julia_session.active = 1;
        g_ctx->view = (ViewState){JULIA_CENTER_RE, JULIA_CENTER_IM, JULIA_ZOOM};
        g_ctx->julia_mode = 1;
        g_ctx->history_count = 0;
    } else {
        if (g_ctx->julia_session.active) g_ctx->view = g_ctx->julia_session.mandelbrot_view;
        g_ctx->julia_mode = 0;
        g_ctx->history_count = 0;
    }
    g_ctx->needs_redraw = 1;
}

EMSCRIPTEN_KEEPALIVE
void wasm_adjust_iterations(int delta) {
    if (!g_ctx) return;
    if (g_ctx->max_iterations + delta >= 10 && g_ctx->max_iterations + delta <= MAX_ITERATIONS_LIMIT) {
        g_ctx->max_iterations += delta;
        init_color_palette(g_ctx->max_iterations, g_ctx->palette_idx);
        g_ctx->needs_redraw = 1;
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_resolution(int w, int h) {
    if (!g_ctx || w < 1 || h < 1) return;
    g_ctx->win_w = w;
    g_ctx->win_h = h;
    SDL_SetWindowSize(g_ctx->window, w, h);
    if (g_ctx->texture) SDL_DestroyTexture(g_ctx->texture);
    g_ctx->texture = SDL_CreateTexture(g_ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING, w, h);
    g_ctx->needs_redraw = 1;
}

