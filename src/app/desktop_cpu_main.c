#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "config.h"
#include "ini_config.h"
#include "julia.h"
#include "mandelbrot.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"
#include "bookmark.h"
#include "color.h"

typedef struct {
    ViewState mandelbrot_view;
    int active;
} JuliaSession;

#define JULIA_CENTER_RE 0.0
#define JULIA_CENTER_IM 0.0
#define JULIA_ZOOM 4.0

static void calculate_boundaries(double center_re, double center_im, double zoom, int width,
                                 int height, double* re_min, double* re_max, double* im_min,
                                 double* im_max);
static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y,
                        SDL_Color color);
static TTF_Font* load_font(void);
static void print_controls(void);

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("mandelbrot-c 3.0.0\n");
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_controls();
            return 0;
        }
    }

    srand((unsigned)time(NULL));

    /* try to load configuration from file */
    load_config_from_file("settings.txt");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) {
        SDL_Quit();
        return 1;
    }

    int win_w = get_config_window_width();
    int win_h = get_config_window_height();

    SDL_Window* window =
        SDL_CreateWindow("Mandelbrot Explorer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         win_w, win_h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = load_font();

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "fatal: sdl_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        if (font) TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, win_w, win_h);
    if (!texture) {
        fprintf(stderr, "fatal: sdl_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (font) TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    ViewState view = {INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
    ViewState history[MAX_HISTORY_SIZE];
    int history_count = 0;

    TourState m_tour = {TOUR_IDLE, 0, 0, 0, 0, 0, 0, 0, -1};
    JuliaTourState j_tour = {JULIA_TOUR_IDLE, 0, 0, 0, 0, 0, -1};

    int julia_mode = 0, burning_ship_mode = 0;
    complex_t julia_c = {-0.7, 0.27};
    JuliaSession julia_session = {{0}, 0};

    int max_iterations = get_config_default_iterations();
    int palette_idx = get_config_default_palette();
    int running = 1;
    int needs_redraw = 1;
    int is_panning = 0, is_zooming = 0;
    int last_mouse_x = 0, last_mouse_y = 0;
    int mouse_x = 0, mouse_y = 0;
    SDL_Rect zoom_rect = {0};
    Uint32 render_time = 0;
    int screenshot_requested = 0;
    int mega_screenshot_requested = 0;
    int current_bookmark_idx = -1;
    int cpu_precision_128 = 0;

    init_renderer(max_iterations, palette_idx);
    set_cpu_precision(cpu_precision_128);
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

                        if (win_w < 1) win_w = 1;
                        if (win_h < 1) win_h = 1;

                        SDL_DestroyTexture(texture);
                        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                    SDL_TEXTUREACCESS_STREAMING, win_w, win_h);
                        if (!texture) {
                            fprintf(stderr, "warning: failed to re-create texture during resize\n");
                        }
                        needs_redraw = 1;
                    }
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                        running = 0;

                    } else if (event.key.keysym.sym == SDLK_z && (SDL_GetModState() & KMOD_CTRL)) {
                        if (m_tour.phase == TOUR_IDLE && j_tour.phase == JULIA_TOUR_IDLE &&
                            history_count > 0)
                            view = history[--history_count];
                        needs_redraw = 1;

                    } else if (event.key.keysym.sym == SDLK_r) {
                        julia_mode = 0;
                        julia_session.active = 0;
                        m_tour.phase = TOUR_IDLE;
                        j_tour.phase = JULIA_TOUR_IDLE;
                        max_iterations = get_config_default_iterations();
                        /* keep palette_idx as is */
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
                            calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w,
                                                 win_h, &re_min, &re_max, &im_min, &im_max);
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

                    } else if (event.key.keysym.sym == SDLK_b) {
                        burning_ship_mode = !burning_ship_mode;
                        julia_mode = 0;
                        julia_session.active = 0;
                        m_tour.phase = TOUR_IDLE;
                        j_tour.phase = JULIA_TOUR_IDLE;
                        view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                        history_count = 0;
                        SDL_SetWindowTitle(window, burning_ship_mode ? "Burning Ship Explorer" : "Mandelbrot Explorer");
                        needs_redraw = 1;

                    } else if (event.key.keysym.sym == SDLK_s) {
                        screenshot_requested = 1;

                    } else if (event.key.keysym.sym == SDLK_x) {
                        mega_screenshot_requested = 1;

                    } else if (event.key.keysym.sym == SDLK_v) {
                        if (is_video_recording()) {
                            stop_video_recording();
                        } else {
                            start_video_recording(win_w, win_h, 60); /* default 60fps for now */
                        }

                    } else if (event.key.keysym.sym == SDLK_m) {
                        Bookmark b = {
                            .center_re = view.center_re,
                            .center_im = view.center_im,
                            .zoom = view.zoom,
                            .max_iterations = max_iterations,
                            .fractal_type = julia_mode ? 1 : (burning_ship_mode ? 2 : 0),
                            .julia_c = julia_c
                        };
                        save_bookmark(&b);
                        printf("Bookmark saved!\n");
                    } else if (event.key.keysym.sym == SDLK_l) {
                        Bookmark b;
                        int count = get_bookmark_count();
                        if (count > 0) {
                            if (history_count < MAX_HISTORY_SIZE) history[history_count++] = view;
                            current_bookmark_idx = (current_bookmark_idx + 1) % count;
                            if (load_bookmark(current_bookmark_idx, &b)) {
                                view.center_re = b.center_re;
                                view.center_im = b.center_im;
                                view.zoom = b.zoom;
                                max_iterations = b.max_iterations;
                                julia_mode = (b.fractal_type == 1);
                                burning_ship_mode = (b.fractal_type == 2);
                                julia_c = b.julia_c;
                                m_tour.phase = TOUR_IDLE;
                                j_tour.phase = JULIA_TOUR_IDLE;
                                init_renderer(max_iterations, palette_idx);
                                needs_redraw = 1;
                                printf("Loaded bookmark %d/%d\n", current_bookmark_idx + 1, count);
                            }
                        }
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        int step = max_iterations / 10;
                        if (step < 10) step = 10;
                        if (SDL_GetModState() & KMOD_SHIFT) step *= 10;
                        
                        if (max_iterations + step <= get_config_max_iterations_limit()) {
                            max_iterations += step;
                            init_renderer(max_iterations, palette_idx);
                            needs_redraw = 1;
                        }

                    } else if (event.key.keysym.sym == SDLK_DOWN) {
                        int step = max_iterations / 10;
                        if (step < 10) step = 10;
                        if (SDL_GetModState() & KMOD_SHIFT) step *= 10;
                        
                        max_iterations -= step;
                        if (max_iterations < 10) max_iterations = 10;
                        
                        init_renderer(max_iterations, palette_idx);
                        needs_redraw = 1;

                    } else if (event.key.keysym.sym == SDLK_p) {
                        palette_idx = (palette_idx + 1) % PALETTE_COUNT;
                        init_renderer(max_iterations, palette_idx);
                        needs_redraw = 1;

                    } else if (event.key.keysym.sym == SDLK_e) {
#ifdef USE_SIMD_F128
                        cpu_precision_128 = !cpu_precision_128;
                        set_cpu_precision(cpu_precision_128);
                        needs_redraw = 1;
#endif

                    } else if (event.key.keysym.sym == SDLK_t) {
                        if (julia_mode) {
                            if (j_tour.phase == JULIA_TOUR_IDLE) {
                                start_julia_tour(&j_tour, &julia_c, SDL_GetTicks());
                                SDL_SetWindowTitle(window, "Julia Explorer  [Auto-c]");
                            } else {
                                stop_julia_tour(&j_tour);
                                SDL_SetWindowTitle(window, "Julia Explorer");
                                needs_redraw = 1;
                            }

                        } else {
                            if (m_tour.phase == TOUR_IDLE) {
                                julia_mode = 0;
                                julia_session.active = 0;
                                history_count = 0;

                                view =
                                    (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                                start_tour(&m_tour, &view);
                                SDL_SetWindowTitle(window, "Mandelbrot Explorer  [Auto-Zoom]");
                            } else {
                                stop_tour(&m_tour);
                                view =
                                    (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                                history_count = 0;
                                SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                                needs_redraw = 1;
                            }
                        }
                    }
                    break;

                case SDL_MOUSEWHEEL: {
                    double zoom_factor = (event.wheel.y > 0) ? 0.9 : 1.1;
                    if (event.wheel.y == 0) break;

                    if (history_count < MAX_HISTORY_SIZE) history[history_count++] = view;

                    double re_min, re_max, im_min, im_max;
                    calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h,
                                         &re_min, &re_max, &im_min, &im_max);

                    double mouse_re = re_min + (double)mouse_x * (re_max - re_min) / win_w;
                    double mouse_im = im_min + (double)mouse_y * (im_max - im_min) / win_h;

                    view.zoom *= zoom_factor;
                    view.center_re = mouse_re + (view.center_re - mouse_re) * zoom_factor;
                    view.center_im = mouse_im + (view.center_im - mouse_im) * zoom_factor;
                    needs_redraw = 1;
                } break;

                case SDL_MOUSEBUTTONDOWN:
                    if (m_tour.phase != TOUR_IDLE) {
                        stop_tour(&m_tour);
                        view = (ViewState){INITIAL_CENTER_RE, INITIAL_CENTER_IM, INITIAL_ZOOM};
                        history_count = 0;
                        SDL_SetWindowTitle(window, "Mandelbrot Explorer");
                        needs_redraw = 1;
                    }
                    if (j_tour.phase != JULIA_TOUR_IDLE) {
                        stop_julia_tour(&j_tour);
                        SDL_SetWindowTitle(window, "Julia Explorer");
                    }
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
                        calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w,
                                             win_h, &re_min, &re_max, &im_min, &im_max);
                        view.center_re -=
                            (event.motion.x - last_mouse_x) * (re_max - re_min) / win_w;
                        view.center_im -=
                            (event.motion.y - last_mouse_y) * (im_max - im_min) / win_h;
                        last_mouse_x = event.motion.x;
                        last_mouse_y = event.motion.y;
                        needs_redraw = 1;
                    } else if (is_zooming) {
                        zoom_rect.w = event.motion.x - zoom_rect.x;
                        zoom_rect.h = event.motion.y - zoom_rect.y;
                    } else if (julia_mode && j_tour.phase == JULIA_TOUR_IDLE) {
                        double re_min, re_max, im_min, im_max;
                        calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w,
                                             win_h, &re_min, &re_max, &im_min, &im_max);
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
                            calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w,
                                                 win_h, &re_min, &re_max, &im_min, &im_max);
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
            update_tour(&m_tour, &view, now, burning_ship_mode);
            needs_redraw = 1;
        }
        if (j_tour.phase != JULIA_TOUR_IDLE) {
            update_julia_tour(&j_tour, &julia_c, now);
            needs_redraw = 1;
        }

        if (needs_redraw) {
            Uint32 start = SDL_GetTicks();
            Uint32* pixels;
            int pitch;
            SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);

            double re_min, re_max, im_min, im_max;
            calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min,
                                 &re_max, &im_min, &im_max);

            if (julia_mode)
                render_julia_threaded(pixels, pitch, win_w, win_h, re_min, re_max, im_max, im_min,
                                      julia_c, max_iterations);
            else if (burning_ship_mode)
                render_burning_ship_threaded(pixels, pitch, win_w, win_h, re_min, re_max, im_max,
                                             im_min, max_iterations);
            else
                render_mandelbrot_threaded(pixels, pitch, win_w, win_h, re_min, re_max, im_max,
                                           im_min, max_iterations);

            SDL_UnlockTexture(texture);
            render_time = SDL_GetTicks() - start;
            needs_redraw = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        if (screenshot_requested) {
            uint32_t* ss_pixels = malloc((size_t)win_w * win_h * 4);
            if (ss_pixels) {
                SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, ss_pixels,
                                     win_w * 4);
                save_screenshot(ss_pixels, win_w, win_h);
                free(ss_pixels);
            }
            screenshot_requested = 0;
        }

        if (mega_screenshot_requested) {
            double re_min, re_max, im_min, im_max;
            calculate_boundaries(view.center_re, view.center_im, view.zoom, win_w, win_h, &re_min,
                                 &re_max, &im_min, &im_max);
            
            /* render a massive 8192 x 8192 screenshot */
            int mega_w = 8192;
            int mega_h = 8192;
            int f_type = julia_mode ? 1 : (burning_ship_mode ? 2 : 0);
            
            save_mega_screenshot(mega_w, mega_h, re_min, re_max, im_min, im_max, max_iterations, 
                                 palette_idx, f_type, julia_c);
            
            mega_screenshot_requested = 0;
            needs_redraw = 1; /* redraw to clear any visual artifacts and reset the renderer state */
        }

        if (is_zooming) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderDrawRect(renderer, &zoom_rect);
        }

        if (DEBUG_INFO && font) {
            char buf[256];
            SDL_Color white = {255, 255, 255, 255};
            int x = 15;
            int y = 12;
            int line_h = FONT_SIZE + 6;
            
            /* calculate dynamic height based on active modes */
            int num_lines = 3; /* base lines (3 info lines) */
            if (m_tour.phase != TOUR_IDLE) num_lines++;
            if (j_tour.phase != JULIA_TOUR_IDLE) num_lines++;
            if (is_video_recording()) num_lines++;
            
            /* draw panel background */
            SDL_Rect bg = {5, 5, 700, num_lines * line_h + 20};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 20, 20, 25, 220); 
            SDL_RenderFillRect(renderer, &bg);

            /* line 1: engine | mode | threads | render */
            int num_threads = get_actual_thread_count();
#ifdef USE_SIMD_F128
            snprintf(buf, sizeof(buf), "[ENGINE] CPU (%s) | Mode: %s | Threads: %d | Render: %u ms",
                     cpu_precision_128 ? "128-bit" : "64-bit",
                     julia_mode ? "Julia" : (burning_ship_mode ? "Burning Ship" : "Mandelbrot"), num_threads, render_time);
#else
            snprintf(buf, sizeof(buf), "[ENGINE] CPU (64-bit) | Mode: %s | Threads: %d | Render: %u ms",
                     julia_mode ? "Julia" : (burning_ship_mode ? "Burning Ship" : "Mandelbrot"), num_threads, render_time);
#endif
            render_text(renderer, font, buf, x, y, white);
            y += line_h;

            /* line 2: coordinates */
            if (julia_mode) {
                snprintf(buf, sizeof(buf), "[COORD]  C: (%.14f, %.14f)", julia_c.re, julia_c.im);
            } else {
                snprintf(buf, sizeof(buf), "[COORD]  Center: (%.14f, %.14f)", view.center_re, view.center_im);
            }
            render_text(renderer, font, buf, x, y, white);
            y += line_h;

            /* line 3: render params */
            snprintf(buf, sizeof(buf), "[RENDER] Zoom: %.6g | Iters: %d | Palette: %s", view.zoom,
                     max_iterations, PALETTE_NAMES[palette_idx % PALETTE_COUNT]);
            render_text(renderer, font, buf, x, y, white);
            y += line_h;

            if (m_tour.phase != TOUR_IDLE) {
                int t_idx = get_tour_target_idx(&m_tour);
                int t_tot = get_num_tour_targets(burning_ship_mode);
                double t_re = get_tour_target_re(&m_tour, burning_ship_mode);
                double t_im = get_tour_target_im(&m_tour, burning_ship_mode);
                snprintf(buf, sizeof(buf), "[TOUR]   Auto-Zoom [%s] Target #%d/%d (%.4f, %.4f)",
                         get_tour_phase_name(m_tour.phase), t_idx + 1, t_tot, t_re, t_im);
                render_text(renderer, font, buf, x, y, white);
                y += line_h;
            }
            if (j_tour.phase != JULIA_TOUR_IDLE) {
                snprintf(buf, sizeof(buf), "[TOUR]   Auto-C [%s] #%d",
                         j_tour.phase == JULIA_TOUR_MOVING ? "moving" : "dwelling",
                         get_julia_tour_target_idx(&j_tour) + 1);
                render_text(renderer, font, buf, x, y, white);
                y += line_h;
            }
            
            if (is_video_recording()) {
                snprintf(buf, sizeof(buf), "[REC] recording video...");
                SDL_Color red = {255, 50, 50, 255};
                render_text(renderer, font, buf, x, y, red);
            }
        }

        SDL_RenderPresent(renderer);
        
        if (is_video_recording()) {
            /* to record what is actually on the screen (including HUD), we have to read pixels back from SDL renderer.
               this is slow, but acceptable for a tool feature. */
            uint32_t* frame_pixels = malloc((size_t)win_w * win_h * 4);
            if (frame_pixels) {
                SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, frame_pixels, win_w * 4);
                append_video_frame(frame_pixels, win_w, win_h);
                free(frame_pixels);
            }
        }
    }

    if (is_video_recording()) stop_video_recording();
    if (font) TTF_CloseFont(font);
    cleanup_renderer();
    cleanup_color_palette();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

static void calculate_boundaries(double center_re, double center_im, double zoom, int width,
                                 int height, double* re_min, double* re_max, double* im_min,
                                 double* im_max) {
    if (height <= 0) height = 1;
    double aspect = (double)width / (double)height;
    *im_min = center_im - zoom / 2.0;
    *im_max = center_im + zoom / 2.0;
    *re_min = center_re - (zoom * aspect) / 2.0;
    *re_max = center_re + (zoom * aspect) / 2.0;
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

static TTF_Font* load_font(void) {
    const char* paths[] = {FONT_PATH_LOCAL, FONT_PATH_1, FONT_PATH_2,
                           FONT_PATH_3,     FONT_PATH_4, NULL};
    for (int i = 0; paths[i] && paths[i][0]; i++) {
        TTF_Font* f = TTF_OpenFont(paths[i], FONT_SIZE);
        if (f) return f;
    }
    return NULL;
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
    puts("  q / esc     : quit");
}