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
            printf("mandelbrot-c 3.0.0\n"); return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_controls(); return 0;
        }
    }

    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) { SDL_Quit(); return 1; }

    int win_w = WINDOW_WIDTH;
    int win_h = WINDOW_HEIGHT;

    SDL_Window *window = SDL_CreateWindow(
        "Mandelbrot Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        TTF_Quit(); SDL_Quit();
        return 1;
    }

    TTF_Font *font = load_font();


    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Fatal: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        if (font) TTF_CloseFont(font);
        TTF_Quit(); SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, win_w, win_h);
    if (!texture) {
        fprintf(stderr, "Fatal: SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (font) TTF_CloseFont(font);
        TTF_Quit(); SDL_Quit();
        return 1;
    }

    // view state
    ViewState view          = {INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ViewState history[MAX_HISTORY_SIZE];
    int       history_count = 0;

    // tour state
    TourState      m_tour = {TOUR_IDLE, 0,0,0, 0,0,0, 0, -1};
    JuliaTourState j_tour = {JULIA_TOUR_IDLE, 0,0, 0,0, 0, -1};

    // julia state
    int          julia_mode    = 0;
    complex_t    julia_c       = {-0.7, 0.27};
    JuliaSession julia_session = {{0}, 0};

    int      max_iterations = DEFAULT_ITERATIONS;
    int      palette_idx    = 0;
    int      running      = 1;
    int      needs_redraw = 1;
    int      is_panning   = 0, is_zooming = 0;
    int      last_mouse_x = 0, last_mouse_y = 0;
    int      mouse_x = 0, mouse_y = 0;
    SDL_Rect zoom_rect   = {0};
    Uint32   render_time = 0;

    init_renderer(max_iterations, palette_idx);
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

                    // safety check
                    if (win_w < 1) win_w = 1;
                    if (win_h < 1) win_h = 1;

                    SDL_DestroyTexture(texture);
                    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, win_w, win_h);
                    if (!texture) {
                        fprintf(stderr, "Warning: failed to re-create texture during resize\n");
                    }
                    needs_redraw = 1;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE ||
                    event.key.keysym.sym == SDLK_q) {
                    running = 0;

                } else if (event.key.keysym.sym == SDLK_z &&
                           (SDL_GetModState() & KMOD_CTRL)) {
                    if (m_tour.phase == TOUR_IDLE && j_tour.phase == JULIA_TOUR_IDLE
                        && history_count > 0)
                        view = history[--history_count];
                    needs_redraw = 1;

                } else if (event.key.keysym.sym == SDLK_r) {
                    julia_mode           = 0;
                    julia_session.active = 0;
                    m_tour.phase         = TOUR_IDLE;
                    j_tour.phase         = JULIA_TOUR_IDLE;
                    max_iterations       = DEFAULT_ITERATIONS;
                    palette_idx          = 0;
                    init_renderer(max_iterations, palette_idx);
                    view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    history_count = 0;
                    SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    needs_redraw = 1;

                } else if (event.key.keysym.sym == SDLK_j) {
                    m_tour.phase = TOUR_IDLE;
                    j_tour.phase = JULIA_TOUR_IDLE;
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
                    uint32_t *ss_pixels = malloc(win_w * win_h * 4);
                    if (ss_pixels) {
                        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, ss_pixels, win_w * 4);
                        save_screenshot(ss_pixels, win_w, win_h);
                        free(ss_pixels);
                    }

                } else if (event.key.keysym.sym == SDLK_UP) {
                    int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                    if (max_iterations + step <= MAX_ITERATIONS_LIMIT) {
                        max_iterations += step;
                        init_renderer(max_iterations, palette_idx);
                        needs_redraw = 1;
                    }

                } else if (event.key.keysym.sym == SDLK_DOWN) {
                    int step = (SDL_GetModState() & KMOD_SHIFT) ? 100 : 10;
                    if (max_iterations - step >= 10) {
                        max_iterations -= step;
                        init_renderer(max_iterations, palette_idx);
                        needs_redraw = 1;
                    }

                } else if (event.key.keysym.sym == SDLK_p) {
                    palette_idx = (palette_idx + 1) % PALETTE_COUNT;
                    init_renderer(max_iterations, palette_idx);
                    needs_redraw = 1;

                } else if (event.key.keysym.sym == SDLK_t) {

                    if (julia_mode) {
                        if (j_tour.phase == JULIA_TOUR_IDLE) {
                            j_tour.from_re    = julia_c.re;
                            j_tour.from_im    = julia_c.im;
                            j_tour.phase      = JULIA_TOUR_DWELLING;
                            j_tour.phase_start = SDL_GetTicks();
                            SDL_SetWindowTitle(window, "Julia Explorer  [Auto-c]");
                        } else {
                            j_tour.phase = JULIA_TOUR_IDLE;
                            SDL_SetWindowTitle(window, "Julia Explorer");
                            needs_redraw = 1;
                        }

                    } else {
                        if (m_tour.phase == TOUR_IDLE) {
                            julia_mode           = 0;
                            julia_session.active = 0;
                            history_count        = 0;

                            m_tour.home_re   = INITIAL_CENTER_RE;
                            m_tour.home_im   = INITIAL_CENTER_IM;
                            m_tour.home_zoom = INITIAL_ZOOM;
                            m_tour.deep_zoom = INITIAL_ZOOM / 6000.0; // TOUR_ZOOM_DEPTH

                            view.center_re = m_tour.home_re;
                            view.center_im = m_tour.home_im;
                            view.zoom      = m_tour.home_zoom;

                            m_tour.phase       = TOUR_ZOOMING_OUT;
                            m_tour.phase_start = SDL_GetTicks() - 10000; 
                            SDL_SetWindowTitle(window, "Mandelbrot Explorer  [Auto-Zoom]");
                        } else {
                            m_tour.phase    = TOUR_IDLE;
                            view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                            history_count = 0;
                            SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                            needs_redraw  = 1;
                        }
                    }
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    double zoom_factor = (event.wheel.y > 0) ? 0.9 : 1.1;
                    if (event.wheel.y == 0) break;

                    if (history_count < MAX_HISTORY_SIZE)
                        history[history_count++] = view;

                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(view.center_re, view.center_im, view.zoom,
                                         win_w, win_h, &re_min, &re_max, &im_min, &im_max);

                    double mouse_re = re_min + (double)mouse_x * (re_max - re_min) / win_w;
                    double mouse_im = im_min + (double)mouse_y * (im_max - im_min) / win_h;

                    view.zoom *= zoom_factor;
                    view.center_re = mouse_re + (view.center_re - mouse_re) * zoom_factor;
                    view.center_im = mouse_im + (view.center_im - mouse_im) * zoom_factor;
                    needs_redraw = 1;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (m_tour.phase != TOUR_IDLE) {
                    m_tour.phase    = TOUR_IDLE;
                    view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                    history_count = 0;
                    SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                    needs_redraw  = 1;
                }
                if (j_tour.phase != JULIA_TOUR_IDLE) {
                    j_tour.phase = JULIA_TOUR_IDLE;
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
                } else if (julia_mode && j_tour.phase == JULIA_TOUR_IDLE) {
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

        Uint32 now = SDL_GetTicks();
        if (m_tour.phase != TOUR_IDLE) {
            update_tour(&m_tour, &view, now);
            needs_redraw = 1;
        }
        if (j_tour.phase != JULIA_TOUR_IDLE) {
            update_julia_tour(&j_tour, &julia_c, now);
            needs_redraw = 1;
        }

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
            int y_start = 5;
            int y = y_start;
            int line_h = FONT_SIZE + 2;
            int num_lines = julia_mode ? 4 : 3;
            if (m_tour.phase != TOUR_IDLE) num_lines++;
            if (j_tour.phase != JULIA_TOUR_IDLE) num_lines++;

            // info box
            SDL_Rect bg = {2, 2, 450, num_lines * line_h + 6};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &bg);

            snprintf(buf, sizeof(buf), "%s | Render: %u ms | Threads: %d",
                     julia_mode ? "JULIA" : "MANDELBROT", render_time, get_actual_thread_count());
            render_text(renderer, font, buf, 5, y, white); y += line_h;
            snprintf(buf, sizeof(buf), "Center: (%.12f, %.12f)",
                     view.center_re, view.center_im);
            render_text(renderer, font, buf, 5, y, white); y += line_h;
            snprintf(buf, sizeof(buf), "Zoom: %.6g | Iterations: %d | Palette: %s",
                     view.zoom, max_iterations, PALETTE_NAMES[palette_idx]);
            render_text(renderer, font, buf, 5, y, white); y += line_h;
            if (julia_mode) {
                snprintf(buf, sizeof(buf), "c = (%.6f, %.6f)", julia_c.re, julia_c.im);
                render_text(renderer, font, buf, 5, y, white); y += line_h;
            }
            if (m_tour.phase != TOUR_IDLE) {
                snprintf(buf, sizeof(buf), "Auto-Zoom [%s]  target #%d",
                         get_tour_phase_name(m_tour.phase), get_tour_target_idx(&m_tour) + 1);
                render_text(renderer, font, buf, 5, y, white); y += line_h;
            }
            if (j_tour.phase != JULIA_TOUR_IDLE) {
                snprintf(buf, sizeof(buf), "Auto-c [%s]  #%d  (%.4f, %.4f)",
                         j_tour.phase == JULIA_TOUR_MOVING ? "moving" : "dwelling",
                         get_julia_tour_target_idx(&j_tour) + 1,
                         julia_c.re, julia_c.im);
                render_text(renderer, font, buf, 5, y, white);
            }
        }

        SDL_RenderPresent(renderer);
    }

    if (font) TTF_CloseFont(font);
    cleanup_renderer();
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
