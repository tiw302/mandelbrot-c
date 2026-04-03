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

// mandelbrot auto-zoom (T key)
// PAN -> ZOOM_IN -> ZOOM_OUT loop
typedef enum {
    TOUR_IDLE = 0,
    TOUR_PANNING,
    TOUR_ZOOMING_IN,
    TOUR_ZOOMING_OUT
} TourPhase;

#define TOUR_ZOOM_DEPTH   6000.0
#define TOUR_PAN_MS       1800.0
#define TOUR_ZOOM_IN_MS   4000.0
#define TOUR_ZOOM_OUT_MS  3200.0

static const struct { double re, im; } ZOOM_TARGETS[] = {
    {-0.743643887074537,  0.131825904145753},
    {-0.162736800339303,  0.878583137739572},
    { 0.275275641098809,  0.006942671571179},
    {-0.458345355141416, -0.633156886463435},
    {-0.761574,  -0.0848},
    {-1.250066,   0.02},
    { 0.001643721, 0.822467633},
    {-0.170337,  -1.065156},
    {-1.768778833,-0.001738996},
    {-0.748,      0.1},
};
#define NUM_ZOOM_TARGETS (int)(sizeof(ZOOM_TARGETS)/sizeof(ZOOM_TARGETS[0]))

static const char *PHASE_NAMES[] = { "", "Panning", "Zooming in", "Zooming out" };

// julia c-parameter tour (T key in julia mode)
#define JULIA_TOUR_MOVE_MS   3000.0
#define JULIA_TOUR_DWELL_MS  1200.0

static const struct { double re, im; } JULIA_C_TARGETS[] = {
    {-0.7000,  0.2700},  // classic spiral
    {-0.4000,  0.6000},  // rabbit
    { 0.2850,  0.0100},  // coral
    {-0.7269,  0.1889},  // siegel disk
    {-0.8000,  0.1560},  // dendrite
    {-0.1200, -0.7700},  // san marco dragon
    { 0.3000, -0.5000},  // islands
    {-0.5400,  0.5400},  // star
    { 0.3700,  0.1000},  // seahorse
    {-0.1940,  0.6557},  // feather
    { 0.0,    0.8},      // cauliflower
    {-0.618,  0.0},      // golden ratio
};
#define NUM_JULIA_C_TARGETS (int)(sizeof(JULIA_C_TARGETS)/sizeof(JULIA_C_TARGETS[0]))

typedef enum {
    JULIA_TOUR_IDLE = 0,
    JULIA_TOUR_MOVING,
    JULIA_TOUR_DWELLING
} JuliaTourPhase;

static void calculate_boundaries(double center_re, double center_im, double zoom,
                                  int width, int height,
                                  double *re_min, double *re_max,
                                  double *im_min, double *im_max);
static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                         const char *text, int x, int y, SDL_Color color);
static TTF_Font *load_font(void);
static void print_controls(void);

static inline double smoothstep(double t) { return t * t * (3.0 - 2.0 * t); }

static int pick_idx(int last, int count) {
    int idx;
    do { idx = rand() % count; } while (idx == last && count > 1);
    return idx;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("mandelbrot-c 2.2.0\n"); return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_controls(); return 0;
        }
    }

    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) { SDL_Quit(); return 1; }

    TTF_Font *font = DEBUG_INFO ? load_font() : NULL;
    int win_w = WINDOW_WIDTH;
    int win_h = WINDOW_HEIGHT;

    SDL_Window *window = SDL_CreateWindow(
        "Mandelbrot Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) return 1;

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) return 1;

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, win_w, win_h);

    // view state
    ViewState view          = {INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ViewState history[MAX_HISTORY_SIZE];
    int       history_count = 0;

    // mandelbrot tour state
    TourPhase tour_phase       = TOUR_IDLE;
    double    tour_home_re     = INITIAL_CENTER_RE;
    double    tour_home_im     = INITIAL_CENTER_IM;
    double    tour_home_zoom   = INITIAL_ZOOM;
    double    tour_target_re   = 0.0;
    double    tour_target_im   = 0.0;
    double    tour_deep_zoom   = 0.0;
    Uint32    tour_phase_start = 0;
    int       last_zoom_idx    = -1;

    // julia tour state
    JuliaTourPhase julia_tour       = JULIA_TOUR_IDLE;
    double         julia_tour_from_re = 0.0;
    double         julia_tour_from_im = 0.0;
    double         julia_tour_to_re   = 0.0;
    double         julia_tour_to_im   = 0.0;
    Uint32         julia_tour_start   = 0;
    int            last_julia_idx     = -1;

    // julia mode state
    int          julia_mode    = 0;
    complex_t    julia_c       = {-0.7, 0.27};
    JuliaSession julia_session = {{0}, 0};

    int      max_iterations = DEFAULT_ITERATIONS;
    int      running      = 1;
    int      needs_redraw = 1;
    int      is_panning   = 0, is_zooming = 0;
    int      last_mouse_x = 0, last_mouse_y = 0;
    int      mouse_x = 0, mouse_y = 0;
    SDL_Rect zoom_rect   = {0};
    Uint32   render_time = 0;

    init_renderer(max_iterations);
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
                if (event.key.keysym.sym == SDLK_ESCAPE ||
                    event.key.keysym.sym == SDLK_q) {
                    running = 0;

                } else if (event.key.keysym.sym == SDLK_z &&
                           (SDL_GetModState() & KMOD_CTRL)) {
                    if (tour_phase == TOUR_IDLE && julia_tour == JULIA_TOUR_IDLE
                        && history_count > 0)
                        view = history[--history_count];
                    needs_redraw = 1;

                } else if (event.key.keysym.sym == SDLK_r) {
                    julia_mode           = 0;
                    julia_session.active = 0;
                    tour_phase           = TOUR_IDLE;
                    julia_tour           = JULIA_TOUR_IDLE;
                    max_iterations       = DEFAULT_ITERATIONS;
                    init_renderer(max_iterations);
                    view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    history_count = 0;
                    SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    needs_redraw = 1;

                } else if (event.key.keysym.sym == SDLK_j) {
                    tour_phase = TOUR_IDLE;
                    julia_tour = JULIA_TOUR_IDLE;
                    if (!julia_mode) {
                        julia_session.mandelbrot_view = view;
                        julia_session.active = 1;
                        double re_min, re_max, im_min, im_max;
                        calculate_boundaries(view.center_re, view.center_im, view.zoom,
                                             win_w, win_h,
                                             &re_min, &re_max, &im_min, &im_max);
                        julia_c.re = re_min + (double)mouse_x * (re_max - re_min) / win_w;
                        julia_c.im = im_min + (double)mouse_y * (im_max - im_min) / win_h;
                        view       = (ViewState){JULIA_CENTER_RE, JULIA_CENTER_IM, JULIA_ZOOM};
                        julia_mode    = 1;
                        history_count = 0;
                        SDL_SetWindowTitle(window, "Julia Explorer");
                    } else {
                        if (julia_session.active) view = julia_session.mandelbrot_view;
                        julia_mode    = 0;
                        history_count = 0;
                        SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    }
                    needs_redraw = 1;

                } else if (event.key.keysym.sym == SDLK_s) {
                    save_screenshot(renderer, win_w, win_h);

                } else if (event.key.keysym.sym == SDLK_UP) {
                    int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                    if (max_iterations + step <= MAX_ITERATIONS_LIMIT) {
                        max_iterations += step;
                        init_renderer(max_iterations);
                        needs_redraw = 1;
                    }

                } else if (event.key.keysym.sym == SDLK_DOWN) {
                    int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                    if (max_iterations - step >= 10) {
                        max_iterations -= step;
                        init_renderer(max_iterations);
                        needs_redraw = 1;
                    }

                } else if (event.key.keysym.sym == SDLK_t) {

                    if (julia_mode) {
                        // start julia c-tour
                        if (julia_tour == JULIA_TOUR_IDLE) {
                            julia_tour_from_re = julia_c.re;
                            julia_tour_from_im = julia_c.im;
                            last_julia_idx     = pick_idx(-1, NUM_JULIA_C_TARGETS);
                            julia_tour_to_re   = JULIA_C_TARGETS[last_julia_idx].re;
                            julia_tour_to_im   = JULIA_C_TARGETS[last_julia_idx].im;
                            julia_tour         = JULIA_TOUR_MOVING;
                            julia_tour_start   = SDL_GetTicks();
                            SDL_SetWindowTitle(window, "Julia Explorer  [Auto-c]");
                        } else {
                            julia_tour = JULIA_TOUR_IDLE;
                            SDL_SetWindowTitle(window, "Julia Explorer");
                            needs_redraw = 1;
                        }

                    } else {
                        // start mandelbrot zoom-tour
                        if (tour_phase == TOUR_IDLE) {
                            julia_mode           = 0;
                            julia_session.active = 0;
                            history_count        = 0;

                            tour_home_re   = INITIAL_CENTER_RE;
                            tour_home_im   = INITIAL_CENTER_IM;
                            tour_home_zoom = INITIAL_ZOOM;
                            tour_deep_zoom = INITIAL_ZOOM / TOUR_ZOOM_DEPTH;

                            view.center_re = tour_home_re;
                            view.center_im = tour_home_im;
                            view.zoom      = tour_home_zoom;

                            last_zoom_idx    = pick_idx(-1, NUM_ZOOM_TARGETS);
                            tour_target_re   = ZOOM_TARGETS[last_zoom_idx].re;
                            tour_target_im   = ZOOM_TARGETS[last_zoom_idx].im;
                            tour_phase       = TOUR_PANNING;
                            tour_phase_start = SDL_GetTicks();
                            SDL_SetWindowTitle(window, "Mandelbrot Explorer  [Auto-Zoom]");
                        } else {
                            tour_phase    = TOUR_IDLE;
                            view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                            history_count = 0;
                            SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                            needs_redraw  = 1;
                        }
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (tour_phase != TOUR_IDLE) {
                    tour_phase    = TOUR_IDLE;
                    view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    history_count = 0;
                    SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    needs_redraw  = 1;
                }
                if (julia_tour != JULIA_TOUR_IDLE) {
                    julia_tour = JULIA_TOUR_IDLE;
                    SDL_SetWindowTitle(window, "Julia Explorer");
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    is_panning   = 1;
                    last_mouse_x = event.button.x;
                    last_mouse_y = event.button.y;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    is_zooming = 1;
                    zoom_rect  = (SDL_Rect){event.button.x, event.button.y, 0, 0};
                }
                break;

            case SDL_MOUSEMOTION:
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
                if (is_panning) {
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(view.center_re, view.center_im, view.zoom,
                                         win_w, win_h, &re_min, &re_max, &im_min, &im_max);
                    view.center_re -= (event.motion.x - last_mouse_x) * (re_max - re_min) / win_w;
                    view.center_im -= (event.motion.y - last_mouse_y) * (im_max - im_min) / win_h;
                    last_mouse_x   = event.motion.x;
                    last_mouse_y   = event.motion.y;
                    needs_redraw   = 1;
                } else if (is_zooming) {
                    zoom_rect.w = event.motion.x - zoom_rect.x;
                    zoom_rect.h = event.motion.y - zoom_rect.y;
                } else if (julia_mode && julia_tour == JULIA_TOUR_IDLE) {
                    // update julia c from mouse if tour is off
                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(view.center_re, view.center_im, view.zoom,
                                         win_w, win_h, &re_min, &re_max, &im_min, &im_max);
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
                        if (history_count < MAX_HISTORY_SIZE)
                            history[history_count++] = view;
                        double re_min, re_max, im_min, im_max;
                        calculate_boundaries(view.center_re, view.center_im, view.zoom,
                                             win_w, win_h, &re_min, &re_max, &im_min, &im_max);
                        double re_pp = (re_max - re_min) / win_w;
                        double im_pp = (im_max - im_min) / win_h;
                        view.center_re = re_min + (zoom_rect.x + zoom_rect.w / 2.0) * re_pp;
                        view.center_im = im_min + (zoom_rect.y + zoom_rect.h / 2.0) * im_pp;
                        view.zoom = fmax(fabs((double)zoom_rect.w) * re_pp,
                                         fabs((double)zoom_rect.h) * im_pp);
                        needs_redraw = 1;
                    }
                    is_zooming = 0;
                }
                break;
            }
        }

        // mandelbrot auto-zoom tour logic
        if (tour_phase != TOUR_IDLE) {
            Uint32 now      = SDL_GetTicks();
            double duration = (tour_phase == TOUR_PANNING)   ? TOUR_PAN_MS :
                              (tour_phase == TOUR_ZOOMING_IN) ? TOUR_ZOOM_IN_MS :
                                                                TOUR_ZOOM_OUT_MS;
            double raw_t = fmin((double)(now - tour_phase_start) / duration, 1.0);
            double e     = smoothstep(raw_t);

            switch (tour_phase) {
            case TOUR_PANNING:
                view.center_re = tour_home_re + (tour_target_re - tour_home_re) * e;
                view.center_im = tour_home_im + (tour_target_im - tour_home_im) * e;
                view.zoom      = tour_home_zoom;
                if (raw_t >= 1.0) {
                    view.center_re   = tour_target_re;
                    view.center_im   = tour_target_im;
                    tour_phase       = TOUR_ZOOMING_IN;
                    tour_phase_start = now;
                }
                break;
            case TOUR_ZOOMING_IN:
                view.center_re = tour_target_re;
                view.center_im = tour_target_im;
                view.zoom = exp(log(tour_home_zoom) +
                                (log(tour_deep_zoom) - log(tour_home_zoom)) * e);
                if (raw_t >= 1.0) {
                    view.zoom        = tour_deep_zoom;
                    tour_phase       = TOUR_ZOOMING_OUT;
                    tour_phase_start = now;
                }
                break;
            case TOUR_ZOOMING_OUT:
                view.center_re = tour_target_re + (tour_home_re - tour_target_re) * e;
                view.center_im = tour_target_im + (tour_home_im - tour_target_im) * e;
                view.zoom = exp(log(tour_deep_zoom) +
                                (log(tour_home_zoom) - log(tour_deep_zoom)) * e);
                if (raw_t >= 1.0) {
                    view.center_re   = tour_home_re;
                    view.center_im   = tour_home_im;
                    view.zoom        = tour_home_zoom;
                    last_zoom_idx    = pick_idx(last_zoom_idx, NUM_ZOOM_TARGETS);
                    tour_target_re   = ZOOM_TARGETS[last_zoom_idx].re;
                    tour_target_im   = ZOOM_TARGETS[last_zoom_idx].im;
                    tour_phase       = TOUR_PANNING;
                    tour_phase_start = now;
                }
                break;
            default: break;
            }
            needs_redraw = 1;
        }

        // julia parameter tour logic
        if (julia_tour != JULIA_TOUR_IDLE) {
            Uint32 now = SDL_GetTicks();

            if (julia_tour == JULIA_TOUR_MOVING) {
                double raw_t = fmin((double)(now - julia_tour_start) / JULIA_TOUR_MOVE_MS, 1.0);
                double e     = smoothstep(raw_t);
                julia_c.re = julia_tour_from_re + (julia_tour_to_re - julia_tour_from_re) * e;
                julia_c.im = julia_tour_from_im + (julia_tour_to_im - julia_tour_from_im) * e;
                if (raw_t >= 1.0) {
                    julia_c.re     = julia_tour_to_re;
                    julia_c.im     = julia_tour_to_im;
                    julia_tour     = JULIA_TOUR_DWELLING;
                    julia_tour_start = now;
                }
            } else { // julia_tour_dwelling
                if ((double)(now - julia_tour_start) >= JULIA_TOUR_DWELL_MS) {
                    julia_tour_from_re = julia_c.re;
                    julia_tour_from_im = julia_c.im;
                    last_julia_idx     = pick_idx(last_julia_idx, NUM_JULIA_C_TARGETS);
                    julia_tour_to_re   = JULIA_C_TARGETS[last_julia_idx].re;
                    julia_tour_to_im   = JULIA_C_TARGETS[last_julia_idx].im;
                    julia_tour         = JULIA_TOUR_MOVING;
                    julia_tour_start   = now;
                }
            }
            needs_redraw = 1;
        }

        // rendering logic
        if (needs_redraw) {
            Uint32 start = SDL_GetTicks();
            Uint32 *pixels; int pitch;
            SDL_LockTexture(texture, NULL, (void **)&pixels, &pitch);

            double re_min, re_max, im_min, im_max;
            calculate_boundaries(view.center_re, view.center_im, view.zoom,
                                  win_w, win_h, &re_min, &re_max, &im_min, &im_max);

            if (julia_mode)
                render_julia_threaded(pixels, pitch, win_w, win_h,
                                      re_min, re_max, im_min, im_max, julia_c, max_iterations);
            else
                render_mandelbrot_threaded(pixels, pitch, win_w, win_h,
                                           re_min, re_max, im_min, im_max, max_iterations);

            SDL_UnlockTexture(texture);
            render_time  = SDL_GetTicks() - start;
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
            char buf[256];
            SDL_Color white = {255, 255, 255, 255};
            int y = 5;
            snprintf(buf, sizeof(buf), "%s | Render: %u ms | Threads: %d",
                     julia_mode ? "JULIA" : "MANDELBROT", render_time, THREAD_COUNT);
            render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            snprintf(buf, sizeof(buf), "Center: (%.12f, %.12f)",
                     view.center_re, view.center_im);
            render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            snprintf(buf, sizeof(buf), "Zoom: %.6g", view.zoom);
            render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            if (julia_mode) {
                snprintf(buf, sizeof(buf), "c = (%.6f, %.6f)", julia_c.re, julia_c.im);
                render_text(renderer, font, buf, 5, y, white); y += FONT_SIZE + 2;
            }
            if (tour_phase != TOUR_IDLE) {
                snprintf(buf, sizeof(buf), "Auto-Zoom [%s]  target #%d",
                         PHASE_NAMES[tour_phase], last_zoom_idx + 1);
                render_text(renderer, font, buf, 5, y, white);
            }
            if (julia_tour != JULIA_TOUR_IDLE) {
                snprintf(buf, sizeof(buf), "Auto-c [%s]  #%d  (%.4f, %.4f)",
                         julia_tour == JULIA_TOUR_MOVING ? "moving" : "dwelling",
                         last_julia_idx + 1,
                         julia_c.re, julia_c.im);
                render_text(renderer, font, buf, 5, y, white);
            }
        }

        SDL_RenderPresent(renderer);
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
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
    puts("  Left Drag       : Zoom into selection");
    puts("  Right Drag      : Pan");
    puts("  Up / Down       : Adjust iterations (+/- 10, Shift for +/- 100)");
    puts("  Ctrl+Z          : Undo zoom");
    puts("  R               : Reset view / iterations");
    puts("  J               : Toggle Julia mode");
    puts("  S               : Save screenshot");
    puts("  T (Mandelbrot)  : Toggle auto-zoom tour");
    puts("  T (Julia)       : Toggle auto-c tour (animates c parameter)");
    puts("  Q / ESC         : Quit");
}
