#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "config.h"
#include "mandelbrot.h"
#include "julia.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"

typedef struct {
    ViewState mandelbrot_view;
    int       active;
} JuliaSession;

// app context
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    TTF_Font     *font;
    int           win_w, win_h;

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
    int      running;
    int      needs_redraw;
    int      is_panning, is_zooming;
    int      last_mouse_x, last_mouse_y;
    int      mouse_x, mouse_y;
    SDL_Rect zoom_rect;
    Uint32   render_time;
} GlobalCtx;

#define JULIA_CENTER_RE   0.0
#define JULIA_CENTER_IM   0.0
#define JULIA_ZOOM        4.0

static void calculate_boundaries(double center_re, double center_im, double zoom,
                                  int width, int height,
                                  double *re_min, double *re_max,
                                  double *im_min, double *im_max);
static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                         const char *text, int x, int y, SDL_Color color);
static TTF_Font *load_font(void);
static void print_controls(void);
void main_loop_iteration(void *arg);

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("mandelbrot-c 3.0.0\n"); return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_controls(); return 0;
        }
    }

    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) { SDL_Quit(); return 1; }

    GlobalCtx *ctx = calloc(1, sizeof(GlobalCtx));
    if (!ctx) { TTF_Quit(); SDL_Quit(); return 1; }

    ctx->font = DEBUG_INFO ? load_font() : NULL;
    ctx->win_w = WINDOW_WIDTH;
    ctx->win_h = WINDOW_HEIGHT;

    ctx->window = SDL_CreateWindow(
        "Mandelbrot Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ctx->win_w, ctx->win_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!ctx->window) {
        fprintf(stderr, "Fatal: SDL_CreateWindow failed: %s\n", SDL_GetError());
        if (ctx->font) TTF_CloseFont(ctx->font);
        free(ctx); TTF_Quit(); SDL_Quit(); return 1;
    }

    ctx->renderer = SDL_CreateRenderer(
        ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
        fprintf(stderr, "Fatal: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ctx->window);
        if (ctx->font) TTF_CloseFont(ctx->font);
        free(ctx); TTF_Quit(); SDL_Quit(); return 1;
    }
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    ctx->texture = SDL_CreateTexture(
        ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, ctx->win_w, ctx->win_h);
    if (!ctx->texture) {
        fprintf(stderr, "Fatal: SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        if (ctx->font) TTF_CloseFont(ctx->font);
        free(ctx); TTF_Quit(); SDL_Quit(); return 1;
    }

    ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ctx->m_tour = (TourState){TOUR_IDLE, 0,0,0, 0,0,0, 0, -1};
    ctx->j_tour = (JuliaTourState){JULIA_TOUR_IDLE, 0,0, 0,0, 0, -1};
    ctx->julia_c = (complex_t){-0.7, 0.27};
    ctx->max_iterations = DEFAULT_ITERATIONS;
    ctx->running = 1;
    ctx->needs_redraw = 1;

    init_renderer(ctx->max_iterations, ctx->palette_idx);
    print_controls();

    while (ctx->running) {
        main_loop_iteration(ctx);
    }

    if (ctx->font) TTF_CloseFont(ctx->font);
    cleanup_renderer();
    SDL_DestroyTexture(ctx->texture);
    SDL_DestroyRenderer(ctx->renderer);
    SDL_DestroyWindow(ctx->window);
    free(ctx);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

void main_loop_iteration(void *arg) {
    GlobalCtx *ctx = (GlobalCtx *)arg;
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
                if (ctx->win_w < 1) ctx->win_w = 1;
                if (ctx->win_h < 1) ctx->win_h = 1;

                SDL_DestroyTexture(ctx->texture);
                ctx->texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, ctx->win_w, ctx->win_h);
                ctx->needs_redraw = 1;
            }
            break;

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                ctx->running = 0;
            } else if (event.key.keysym.sym == SDLK_z && (SDL_GetModState() & KMOD_CTRL)) {
                if (ctx->m_tour.phase == TOUR_IDLE && ctx->j_tour.phase == JULIA_TOUR_IDLE && ctx->history_count > 0)
                    ctx->view = ctx->history[--ctx->history_count];
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_r) {
                ctx->julia_mode = 0;
                ctx->julia_session.active = 0;
                ctx->m_tour.phase = TOUR_IDLE;
                ctx->j_tour.phase = JULIA_TOUR_IDLE;
                ctx->max_iterations = DEFAULT_ITERATIONS;
                ctx->palette_idx = 0;
                init_renderer(ctx->max_iterations, ctx->palette_idx);
                ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                ctx->history_count = 0;
                SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_j) {
                ctx->m_tour.phase = TOUR_IDLE;
                ctx->j_tour.phase = JULIA_TOUR_IDLE;
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
                    SDL_SetWindowTitle(ctx->window, "Julia Explorer");
                } else {
                    if (ctx->julia_session.active) ctx->view = ctx->julia_session.mandelbrot_view;
                    ctx->julia_mode = 0;
                    ctx->history_count = 0;
                    SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
                }
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_s) {
                save_screenshot(ctx->renderer, ctx->win_w, ctx->win_h);
            } else if (event.key.keysym.sym == SDLK_UP) {
                int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                if (ctx->max_iterations + step <= MAX_ITERATIONS_LIMIT) {
                    ctx->max_iterations += step;
                    init_renderer(ctx->max_iterations, ctx->palette_idx);
                    ctx->needs_redraw = 1;
                }
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                if (ctx->max_iterations - step >= 10) {
                    ctx->max_iterations -= step;
                    init_renderer(ctx->max_iterations, ctx->palette_idx);
                    ctx->needs_redraw = 1;
                }
            } else if (event.key.keysym.sym == SDLK_p) {
                ctx->palette_idx = (ctx->palette_idx + 1) % PALETTE_COUNT;
                init_renderer(ctx->max_iterations, ctx->palette_idx);
                ctx->needs_redraw = 1;
            } else if (event.key.keysym.sym == SDLK_t) {
                if (ctx->julia_mode) {
                    if (ctx->j_tour.phase == JULIA_TOUR_IDLE) {
                        ctx->j_tour.from_re = ctx->julia_c.re;
                        ctx->j_tour.from_im = ctx->julia_c.im;
                        ctx->j_tour.phase = JULIA_TOUR_DWELLING;
                        ctx->j_tour.phase_start = SDL_GetTicks();
                        SDL_SetWindowTitle(ctx->window, "Julia Explorer  [Auto-c]");
                    } else {
                        ctx->j_tour.phase = JULIA_TOUR_IDLE;
                        SDL_SetWindowTitle(ctx->window, "Julia Explorer");
                        ctx->needs_redraw = 1;
                    }
                } else {
                    if (ctx->m_tour.phase == TOUR_IDLE) {
                        ctx->julia_mode = 0;
                        ctx->julia_session.active = 0;
                        ctx->history_count = 0;
                        ctx->m_tour.home_re = INITIAL_CENTER_RE;
                        ctx->m_tour.home_im = INITIAL_CENTER_IM;
                        ctx->m_tour.home_zoom = INITIAL_ZOOM;
                        ctx->m_tour.deep_zoom = INITIAL_ZOOM / 6000.0;
                        ctx->view.center_re = ctx->m_tour.home_re;
                        ctx->view.center_im = ctx->m_tour.home_im;
                        ctx->view.zoom = ctx->m_tour.home_zoom;
                        ctx->m_tour.phase = TOUR_ZOOMING_OUT;
                        ctx->m_tour.phase_start = SDL_GetTicks() - 10000;
                        SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer  [Auto-Zoom]");
                    } else {
                        ctx->m_tour.phase = TOUR_IDLE;
                        ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                        ctx->history_count = 0;
                        SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
                        ctx->needs_redraw = 1;
                    }
                }
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
            if (ctx->m_tour.phase != TOUR_IDLE) {
                ctx->m_tour.phase = TOUR_IDLE;
                ctx->view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                ctx->history_count = 0;
                SDL_SetWindowTitle(ctx->window, "Mandelbrot Explorer");
                ctx->needs_redraw = 1;
            }
            if (ctx->j_tour.phase != JULIA_TOUR_IDLE) {
                ctx->j_tour.phase = JULIA_TOUR_IDLE;
                SDL_SetWindowTitle(ctx->window, "Julia Explorer");
            }
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
            } else if (ctx->julia_mode && ctx->j_tour.phase == JULIA_TOUR_IDLE) {
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

    Uint32 now = SDL_GetTicks();
    if (ctx->m_tour.phase != TOUR_IDLE) {
        update_tour(&ctx->m_tour, &ctx->view, now);
        ctx->needs_redraw = 1;
    }
    if (ctx->j_tour.phase != JULIA_TOUR_IDLE) {
        update_julia_tour(&ctx->j_tour, &ctx->julia_c, now);
        ctx->needs_redraw = 1;
    }

    if (ctx->needs_redraw) {
        Uint32 start = SDL_GetTicks();
        Uint32 *pixels; int pitch;
        SDL_LockTexture(ctx->texture, NULL, (void **)&pixels, &pitch);
        double re_min, re_max, im_min, im_max;
        calculate_boundaries(ctx->view.center_re, ctx->view.center_im, ctx->view.zoom,
                              ctx->win_w, ctx->win_h, &re_min, &re_max, &im_min, &im_max);
        if (ctx->julia_mode)
            render_julia_threaded(pixels, pitch, ctx->win_w, ctx->win_h,
                                  re_min, re_max, im_min, im_max, ctx->julia_c, ctx->max_iterations);
        else
            render_mandelbrot_threaded(pixels, pitch, ctx->win_w, ctx->win_h,
                                       re_min, re_max, im_min, im_max, ctx->max_iterations);
        SDL_UnlockTexture(ctx->texture);
        ctx->render_time = SDL_GetTicks() - start;
        ctx->needs_redraw = 0;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);

    if (ctx->is_zooming) {
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 0, 255);
        SDL_RenderDrawRect(ctx->renderer, &ctx->zoom_rect);
    }

    if (DEBUG_INFO && ctx->font) {
        char buf[256];
        SDL_Color white = {255, 255, 255, 255};
        int y = 5;
        int line_h = FONT_SIZE + 2;
        int num_lines = ctx->julia_mode ? 4 : 3;
        if (ctx->m_tour.phase != TOUR_IDLE) num_lines++;
        if (ctx->j_tour.phase != JULIA_TOUR_IDLE) num_lines++;

        SDL_Rect bg = {2, 2, 450, num_lines * line_h + 6};
        // set transparency
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(ctx->renderer, &bg);

        snprintf(buf, sizeof(buf), "%s | Render: %u ms | Threads: %d",
                 ctx->julia_mode ? "JULIA" : "MANDELBROT", ctx->render_time, get_actual_thread_count());
        render_text(ctx->renderer, ctx->font, buf, 5, y, white); y += line_h;
        snprintf(buf, sizeof(buf), "Center: (%.12f, %.12f)", ctx->view.center_re, ctx->view.center_im);
        render_text(ctx->renderer, ctx->font, buf, 5, y, white); y += line_h;
        snprintf(buf, sizeof(buf), "Zoom: %.6g | Iterations: %d | Palette: %s",
                 ctx->view.zoom, ctx->max_iterations, PALETTE_NAMES[ctx->palette_idx]);
        render_text(ctx->renderer, ctx->font, buf, 5, y, white); y += line_h;
        if (ctx->julia_mode) {
            snprintf(buf, sizeof(buf), "c = (%.6f, %.6f)", ctx->julia_c.re, ctx->julia_c.im);
            render_text(ctx->renderer, ctx->font, buf, 5, y, white); y += line_h;
        }
        if (ctx->m_tour.phase != TOUR_IDLE) {
            snprintf(buf, sizeof(buf), "Auto-Zoom [%s]  target #%d",
                     get_tour_phase_name(ctx->m_tour.phase), get_tour_target_idx(&ctx->m_tour) + 1);
            render_text(ctx->renderer, ctx->font, buf, 5, y, white); y += line_h;
        }
        if (ctx->j_tour.phase != JULIA_TOUR_IDLE) {
            snprintf(buf, sizeof(buf), "Auto-c [%s]  #%d  (%.4f, %.4f)",
                     ctx->j_tour.phase == JULIA_TOUR_MOVING ? "moving" : "dwelling",
                     get_julia_tour_target_idx(&ctx->j_tour) + 1, ctx->julia_c.re, ctx->julia_c.im);
            render_text(ctx->renderer, ctx->font, buf, 5, y, white);
        }
    }
    SDL_RenderPresent(ctx->renderer);


}

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

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                         const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surface);
}

static TTF_Font *load_font(void) {
    const char *paths[] = {FONT_PATH_1, FONT_PATH_2, FONT_PATH_3, FONT_PATH_4, NULL};
    for (int i = 0; paths[i] && paths[i][0]; i++) {
        TTF_Font *f = TTF_OpenFont(paths[i], FONT_SIZE);
        if (f) return f;
    }
    return NULL;
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
