#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "config.h"
#include "mandelbrot.h"
#include "julia.h"
#include "renderer.h"
#include "screenshot.h"

typedef struct {
    double center_re;
    double center_im;
    double zoom;
} ViewState;

typedef struct {
    ViewState mandelbrot_view;
    int       active;
} JuliaSession;

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

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("mandelbrot-c 1.0.0\n");
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_controls();
            return 0;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) { SDL_Quit(); return 1; }

    TTF_Font *font = DEBUG_INFO ? load_font() : NULL;
    int win_w = WINDOW_WIDTH;
    int win_h = WINDOW_HEIGHT;

    SDL_Window *window = SDL_CreateWindow(
        "Mandelbrot Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (!window) return 1;

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) return 1;

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, win_w, win_h
    );

    /* Initial state */
    ViewState view = {INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ViewState history[MAX_HISTORY_SIZE];
    int       history_count = 0;

    int          julia_mode = 0;
    complex_t    julia_c    = {-0.7, 0.27};
    JuliaSession julia_session = {{0}, 0};

    int      running = 1;
    int      needs_redraw = 1;
    int      is_panning = 0, is_zooming = 0;
    int      last_mouse_x = 0, last_mouse_y = 0;
    int      mouse_x = 0, mouse_y = 0;
    SDL_Rect zoom_rect = {0};
    Uint32   render_time = 0;

    init_renderer();
    print_controls();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    win_w = event.window.data1;
                    win_h = event.window.data2;
                    SDL_DestroyTexture(texture);
                    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                               SDL_TEXTUREACCESS_STREAMING, win_w, win_h);
                    needs_redraw = 1;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                    running = 0;
                } else if (event.key.keysym.sym == SDLK_z && (SDL_GetModState() & KMOD_CTRL)) {
                    if (history_count > 0) view = history[--history_count];
                    needs_redraw = 1;
                } else if (event.key.keysym.sym == SDLK_r) {
                    julia_mode = 0;
                    view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    history_count = 0;
                    julia_session.active = 0;
                    SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    needs_redraw = 1;
                } else if (event.key.keysym.sym == SDLK_j) {
                    if (!julia_mode) {
                        julia_session.mandelbrot_view = view;
                        julia_session.active = 1;
                        double re_min, re_max, im_min, im_max;
                        calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min, &re_max, &im_min, &im_max);
                        julia_c.re = re_min + (double)mouse_x * (re_max - re_min) / win_w;
                        julia_c.im = im_min + (double)mouse_y * (im_max - im_min) / win_h;
                        view = (ViewState){JULIA_CENTER_RE, JULIA_CENTER_IM, JULIA_ZOOM};
                        julia_mode = 1;
                        history_count = 0;
                        SDL_SetWindowTitle(window, "Julia Explorer");
                    } else {
                        if (julia_session.active) view = julia_session.mandelbrot_view;
                        julia_mode = 0;
                        history_count = 0;
                        SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    }
                    needs_redraw = 1;
                } else if (event.key.keysym.sym == SDLK_s) {
                    save_screenshot(renderer, win_w, win_h);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    is_panning = 1;
                    last_mouse_x = event.button.x;
                    last_mouse_y = event.button.y;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    is_zooming = 1;
                    zoom_rect = (SDL_Rect){event.button.x, event.button.y, 0, 0};
                }
                break;

            case SDL_MOUSEMOTION:
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
                if (is_panning) {
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min, &re_max, &im_min, &im_max);
                    view.center_re -= (event.motion.x - last_mouse_x) * (re_max - re_min) / win_w;
                    view.center_im -= (event.motion.y - last_mouse_y) * (im_max - im_min) / win_h;
                    last_mouse_x = event.motion.x;
                    last_mouse_y = event.motion.y;
                    needs_redraw = 1;
                } else if (is_zooming) {
                    zoom_rect.w = event.motion.x - zoom_rect.x;
                    zoom_rect.h = event.motion.y - zoom_rect.y;
                } else if (julia_mode) {
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min, &re_max, &im_min, &im_max);
                    julia_c.re = re_min + (double)mouse_x * (re_max - re_min) / win_w;
                    julia_c.im = im_min + (double)mouse_y * (im_max - im_min) / win_h;
                    needs_redraw = 1;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    is_panning = 0;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    if (is_zooming && zoom_rect.w != 0 && zoom_rect.h != 0) {
                        if (history_count < MAX_HISTORY_SIZE) history[history_count++] = view;
                        double re_min, re_max, im_min, im_max;
                        calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min, &re_max, &im_min, &im_max);
                        double re_pp = (re_max - re_min) / win_w;
                        double im_pp = (im_max - im_min) / win_h;
                        view.center_re = re_min + (zoom_rect.x + zoom_rect.w / 2.0) * re_pp;
                        view.center_im = im_min + (zoom_rect.y + zoom_rect.h / 2.0) * im_pp;
                        view.zoom = fmax(fabs((double)zoom_rect.w) * re_pp, fabs((double)zoom_rect.h) * im_pp);
                        needs_redraw = 1;
                    }
                    is_zooming = 0;
                }
                break;
            }
        }

        if (needs_redraw) {
            Uint32 start = SDL_GetTicks();
            Uint32 *pixels; int pitch;
            SDL_LockTexture(texture, NULL, (void **)&pixels, &pitch);

            double re_min, re_max, im_min, im_max;
            calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min, &re_max, &im_min, &im_max);

            if (julia_mode)
                render_julia_threaded(pixels, pitch, win_w, win_h, re_min, re_max, im_min, im_max, julia_c);
            else
                render_mandelbrot_threaded(pixels, pitch, win_w, win_h, re_min, re_max, im_min, im_max);

            SDL_UnlockTexture(texture);
            render_time = SDL_GetTicks() - start;
            needs_redraw = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        if (is_zooming) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderDrawRect(renderer, &zoom_rect);
        }

        if (DEBUG_INFO && font) {
            char buf[256]; SDL_Color white = {255, 255, 255, 255}; int y = 5;
            snprintf(buf, sizeof(buf), "%s | Render: %u ms | Threads: %d", julia_mode ? "JULIA" : "MANDELBROT", render_time, THREAD_COUNT);
            render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            snprintf(buf, sizeof(buf), "Center: (%.12f, %.12f)", view.center_re, view.center_im);
            render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            snprintf(buf, sizeof(buf), "Zoom: %g", view.zoom);
            render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            if (julia_mode) {
                snprintf(buf, sizeof(buf), "c = (%.6f, %.6f)", julia_c.re, julia_c.im);
                render_text(renderer, font, buf, 5, y, white);
            }
        }
        SDL_RenderPresent(renderer);
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit(); SDL_Quit();
    return 0;
}

static void calculate_boundaries(double center_re, double center_im, double zoom,
                                   int width, int height,
                                   double *re_min, double *re_max,
                                   double *im_min, double *im_max) {
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
    puts("  Left Drag  : Zoom");
    puts("  Right Drag : Pan");
    puts("  Ctrl+Z     : Undo Zoom");
    puts("  R          : Reset View");
    puts("  J          : Toggle Julia Mode");
    puts("  S          : Save Screenshot");
    puts("  Q / ESC    : Quit");
}
