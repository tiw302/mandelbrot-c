/* tour.c
 *
 * path generation and camera interpolation for animation tours.
 * calculates smooth transitions between bookmarked coordinates.
 */

#include "tour.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "config.h"
#include "bookmark.h"

// tour configuration constants
#define TOUR_ZOOM_DEPTH 6000.0   // target zoom depth (relative to current)
#define TOUR_PAN_MS 1800.0       // duration of the initial pan movement
#define TOUR_ZOOM_IN_MS 4000.0   // duration of the deep zoom phase
#define TOUR_ZOOM_OUT_MS 3200.0  // duration of the pull-back phase

// preset coordinates for the mandelbrot set tour
static const struct {
    double re, im;
} ZOOM_TARGETS[] = {
    {-0.743643887074537, 0.131825904145753},
    {-0.162736800339303, 0.878583137739572},
    {0.275275641098809, 0.006942671571179},
    {-0.458345355141416, -0.633156886463435},
    {-0.761574, -0.0848},
    {-1.250066, 0.02},
    {0.001643721, 0.822467633},
    {-0.170337, -1.065156},
    {-1.768778833, -0.001738996},
    {-0.748, 0.1},
    {-0.743643135, 0.131825963},
    {0.27322626, 0.59515333},
    {-0.1, 0.838},
    {-0.1528, 1.0397},
    {-1.768778833, 0.0},
    {-1.25066, 0.02012},
    {-0.7453, 0.1127},
    {-0.75, 0.11},
    {-0.1, 0.651},
    {0.2869, 0.0142},
};
#define NUM_ZOOM_TARGETS (int)(sizeof(ZOOM_TARGETS) / sizeof(ZOOM_TARGETS[0]))

// preset coordinates for the burning ship fractal tour
static const struct {
    double re, im;
} SHIP_ZOOM_TARGETS[] = {
    {-1.75, -0.03},
    {-1.86, -0.005},
    {-1.745, -0.04},
    {-1.797, -0.02},
    {-1.861, -0.004},
    {-1.76, -0.01},
    {-1.75482613, -0.02100650},
    {-1.740061, -0.028169},
    {-1.8386, -0.001},
    {-1.745, -0.02},
    {-1.765, -0.015},
    {-1.742, -0.01},
    {-1.78, -0.025},
    {-1.82, -0.015},
};
#define NUM_SHIP_ZOOM_TARGETS (int)(sizeof(SHIP_ZOOM_TARGETS) / sizeof(SHIP_ZOOM_TARGETS[0]))

// preset coordinates for the tricorn fractal tour
static const struct {
    double re, im;
} TRICORN_ZOOM_TARGETS[] = {
    {-0.743643887074537, 0.131825904145753},
    {-0.162736800339303, 0.878583137739572},
    {0.0, 1.0},
    {-1.25, 0.02},
    {-0.458345355141416, -0.633156886463435},
    {-1.768778833, 0.0}
};
#define NUM_TRICORN_ZOOM_TARGETS (int)(sizeof(TRICORN_ZOOM_TARGETS) / sizeof(TRICORN_ZOOM_TARGETS[0]))

// preset coordinates for the celtic fractal tour
static const struct {
    double re, im;
} CELTIC_ZOOM_TARGETS[] = {
    {-0.5, 0.5},
    {-0.162736800339303, 0.878583137739572},
    {0.275275641098809, 0.006942671571179},
    {-0.743643887074537, 0.131825904145753},
    {-0.1, 0.838}
};
#define NUM_CELTIC_ZOOM_TARGETS (int)(sizeof(CELTIC_ZOOM_TARGETS) / sizeof(CELTIC_ZOOM_TARGETS[0]))

// preset coordinates for the buffalo fractal tour
static const struct {
    double re, im;
} BUFFALO_ZOOM_TARGETS[] = {
    {-1.75, -0.03},
    {-1.86, -0.005},
    {-1.745, -0.04},
    {-1.797, -0.02},
    {-0.4, 0.4}
};
#define NUM_BUFFALO_ZOOM_TARGETS (int)(sizeof(BUFFALO_ZOOM_TARGETS) / sizeof(BUFFALO_ZOOM_TARGETS[0]))

#define JULIA_TOUR_MOVE_MS 3000.0   // duration of parameter interpolation
#define JULIA_TOUR_DWELL_MS 1200.0  // duration of pause at keyframes

// preset c-parameter keyframes for the julia set tour
static const struct {
    double re, im;
} JULIA_C_TARGETS[] = {
    {-0.7000, 0.2700}, {-0.4000, 0.6000},  {0.2850, 0.0100},  {-0.7269, 0.1889},
    {-0.8000, 0.1560}, {-0.1200, -0.7700}, {0.3000, -0.5000}, {-0.5400, 0.5400},
    {0.3700, 0.1000},  {-0.1940, 0.6557},  {0.0, 0.8},        {-0.618, 0.0},
};
#define NUM_JULIA_C_TARGETS (int)(sizeof(JULIA_C_TARGETS) / sizeof(JULIA_C_TARGETS[0]))

/* 
 * [MATH] smoothstep interpolation
 * standard hermite interpolation curve. it provides zero velocity at t=0 and t=1.
 * we use this heavily to avoid jarring camera starts and stops between keyframes.
 */
static inline double smoothstep(double t) {
    return t * t * (3.0 - 2.0 * t);
}

/* 
 * [MATH] quadratic bezier interpolation
 * creates a smooth curve rather than a straight line between two points.
 */
static inline double bezier_q(double p0, double p1, double p2, double t) {
    double u = 1.0 - t;
    return u * u * p0 + 2.0 * u * t * p1 + t * t * p2;
}

// random target selection that guarantees we never pick the same target twice in a row
static int pick_idx(int last, int count) {
    if (count <= 0) {
        return 0;  // prevent division by zero
    }
    int idx;
    do {
        idx = rand() % count;
    } while (idx == last && count > 1);
    return idx;
}

/* 
 * [ARCH] dynamic target selection
 * instead of hardcoding camera paths, this function reads user-saved keyframes
 * (bookmarks) directly from the dynamic memory cache and filters them by the
 * currently active fractal type. it then randomly selects the next keyframe.
 */
static int get_dynamic_targets(int base_fractal, double* out_re, double* out_im, double* out_zoom, int* out_idx, int* out_count) {
    int total = 0;
    const Bookmark* bookmarks = get_bookmarks_array(&total);

    if (total <= 0 || !bookmarks) return 0;

    int* candidates_orig_idx = malloc(sizeof(int) * total);
    if (!candidates_orig_idx) {
        return 0;
    }

    int match_count = 0;
    for (int i = 0; i < total; i++) {
        // exclude default view or too zoomed out bookmarks to ensure interesting tour targets
        if (bookmarks[i].fractal_type == base_fractal) {
            if (bookmarks[i].zoom >= 0.05 || (fabs(bookmarks[i].center_re - -0.5) < 1e-4 && fabs(bookmarks[i].center_im - 0.0) < 1e-4)) {
                continue;
            }
            candidates_orig_idx[match_count++] = i;
        }
    }

    if (match_count == 0) {
        free(candidates_orig_idx);
        return 0;
    }

    int r = pick_idx(*out_idx, match_count);
    int selected_idx = candidates_orig_idx[r];
    
    *out_idx = r;
    *out_count = match_count;
    *out_re = bookmarks[selected_idx].center_re;
    *out_im = bookmarks[selected_idx].center_im;
    *out_zoom = bookmarks[selected_idx].zoom;

    free(candidates_orig_idx);
    return 1;
}

// advances the main fractal tour state machine based on current timestamp
void update_tour(TourState* state, ViewState* view, uint32_t now, int base_fractal) {
    if (state->phase == TOUR_IDLE) return;
    if (state->phase_start == 0) {
        state->phase_start = now;
    }

    double duration = (state->phase == TOUR_PANNING)      ? TOUR_PAN_MS
                      : (state->phase == TOUR_ZOOMING_IN) ? TOUR_ZOOM_IN_MS
                                                          : TOUR_ZOOM_OUT_MS;
    int32_t dt = (int32_t)(now - state->phase_start);
    double raw_t = fmax(0.0, fmin((double)dt / duration, 1.0));
    double e = smoothstep(raw_t);

    switch (state->phase) {
        case TOUR_PANNING: {
            // bezier interpolation for cinematic panning (curves outward)
            double mid_re = (state->home_re + state->target_re) * 0.5;
            double mid_im = (state->home_im + state->target_im) * 0.5;
            double dx = state->target_re - state->home_re;
            double dy = state->target_im - state->home_im;
            double cp_re = mid_re - dy * 0.3; // perpendicular offset
            double cp_im = mid_im + dx * 0.3;
            
            view->center_re = bezier_q(state->home_re, cp_re, state->target_re, e);
            view->center_im = bezier_q(state->home_im, cp_im, state->target_im, e);
            view->zoom = state->home_zoom;
            if (raw_t >= 1.0) {
                view->center_re = state->target_re;
                view->center_im = state->target_im;
                state->phase = TOUR_ZOOMING_IN;
                state->phase_start = now;
            }
            break;
        }
        case TOUR_ZOOMING_IN:
            // exponential interpolation for smooth zoom depth perception
            view->center_re = state->target_re;
            view->center_im = state->target_im;
            view->zoom =
                exp(log(state->home_zoom) + (log(state->deep_zoom) - log(state->home_zoom)) * e);
            if (raw_t >= 1.0) {
                view->zoom = state->deep_zoom;
                state->phase = TOUR_ZOOMING_OUT;
                state->phase_start = now;
            }
            break;
        case TOUR_ZOOMING_OUT: {
            // return to home position with an opposing curve
            double mid_re = (state->target_re + state->home_re) * 0.5;
            double mid_im = (state->target_im + state->home_im) * 0.5;
            double dx = state->home_re - state->target_re;
            double dy = state->home_im - state->target_im;
            double cp_re = mid_re - dy * 0.3;
            double cp_im = mid_im + dx * 0.3;
            
            view->center_re = bezier_q(state->target_re, cp_re, state->home_re, e);
            view->center_im = bezier_q(state->target_im, cp_im, state->home_im, e);
            view->zoom =
                exp(log(state->deep_zoom) + (log(state->home_zoom) - log(state->deep_zoom)) * e);
            if (raw_t >= 1.0) {
                view->center_re = state->home_re;
                view->center_im = state->home_im;
                view->zoom = state->home_zoom;

                // select a new random target for the next loop depending on the fractal type
                double dyn_re = 0.0;
                double dyn_im = 0.0;
                double dyn_zoom = 0.0;
                int dyn_idx = 0;
                int dyn_count = 0;
                if (get_dynamic_targets(base_fractal, &dyn_re, &dyn_im, &dyn_zoom, &dyn_idx, &dyn_count)) {
                    state->is_dynamic = 1;
                    state->last_zoom_idx = dyn_idx;
                    state->target_re = dyn_re;
                    state->target_im = dyn_im;
                    if (dyn_zoom < 0.0005) {
                        state->deep_zoom = dyn_zoom;
                    } else {
                        state->deep_zoom = state->home_zoom / TOUR_ZOOM_DEPTH;
                    }
                } else {
                    state->is_dynamic = 0;
                    switch (base_fractal) {
                        case RENDER_BURNING_SHIP:
                            state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_SHIP_ZOOM_TARGETS);
                            state->target_re = SHIP_ZOOM_TARGETS[state->last_zoom_idx].re;
                            state->target_im = SHIP_ZOOM_TARGETS[state->last_zoom_idx].im;
                            break;
                        case RENDER_TRICORN:
                            state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_TRICORN_ZOOM_TARGETS);
                            state->target_re = TRICORN_ZOOM_TARGETS[state->last_zoom_idx].re;
                            state->target_im = TRICORN_ZOOM_TARGETS[state->last_zoom_idx].im;
                            break;
                        case RENDER_CELTIC:
                            state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_CELTIC_ZOOM_TARGETS);
                            state->target_re = CELTIC_ZOOM_TARGETS[state->last_zoom_idx].re;
                            state->target_im = CELTIC_ZOOM_TARGETS[state->last_zoom_idx].im;
                            break;
                        case RENDER_BUFFALO:
                            state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_BUFFALO_ZOOM_TARGETS);
                            state->target_re = BUFFALO_ZOOM_TARGETS[state->last_zoom_idx].re;
                            state->target_im = BUFFALO_ZOOM_TARGETS[state->last_zoom_idx].im;
                            break;
                        default:
                            state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_ZOOM_TARGETS);
                            state->target_re = ZOOM_TARGETS[state->last_zoom_idx].re;
                            state->target_im = ZOOM_TARGETS[state->last_zoom_idx].im;
                            break;
                    }
                    state->deep_zoom = state->home_zoom / TOUR_ZOOM_DEPTH;
                }
                state->phase = TOUR_PANNING;
                state->phase_start = now;
            }
            break;
        }
        default:
            break;
    }
}

// initializes tour state based on the current viewport
void start_tour(TourState* state, ViewState* view, int base_fractal) {
    state->home_re = view->center_re;
    state->home_im = view->center_im;
    state->home_zoom = view->zoom;
    
    // Pick the first target coordinate immediately
    double dyn_re = 0.0;
    double dyn_im = 0.0;
    double dyn_zoom = 0.0;
    int dyn_idx = 0;
    int dyn_count = 0;
    if (get_dynamic_targets(base_fractal, &dyn_re, &dyn_im, &dyn_zoom, &dyn_idx, &dyn_count)) {
        state->is_dynamic = 1;
        state->last_zoom_idx = dyn_idx;
        state->target_re = dyn_re;
        state->target_im = dyn_im;
        if (dyn_zoom < 0.0005) {
            state->deep_zoom = dyn_zoom;
        } else {
            state->deep_zoom = state->home_zoom / TOUR_ZOOM_DEPTH;
        }
    } else {
        state->is_dynamic = 0;
        switch (base_fractal) {
            case RENDER_BURNING_SHIP:
                state->last_zoom_idx = pick_idx(-1, NUM_SHIP_ZOOM_TARGETS);
                state->target_re = SHIP_ZOOM_TARGETS[state->last_zoom_idx].re;
                state->target_im = SHIP_ZOOM_TARGETS[state->last_zoom_idx].im;
                break;
            case RENDER_TRICORN:
                state->last_zoom_idx = pick_idx(-1, NUM_TRICORN_ZOOM_TARGETS);
                state->target_re = TRICORN_ZOOM_TARGETS[state->last_zoom_idx].re;
                state->target_im = TRICORN_ZOOM_TARGETS[state->last_zoom_idx].im;
                break;
            case RENDER_CELTIC:
                state->last_zoom_idx = pick_idx(-1, NUM_CELTIC_ZOOM_TARGETS);
                state->target_re = CELTIC_ZOOM_TARGETS[state->last_zoom_idx].re;
                state->target_im = CELTIC_ZOOM_TARGETS[state->last_zoom_idx].im;
                break;
            case RENDER_BUFFALO:
                state->last_zoom_idx = pick_idx(-1, NUM_BUFFALO_ZOOM_TARGETS);
                state->target_re = BUFFALO_ZOOM_TARGETS[state->last_zoom_idx].re;
                state->target_im = BUFFALO_ZOOM_TARGETS[state->last_zoom_idx].im;
                break;
            default:
                state->last_zoom_idx = pick_idx(-1, NUM_ZOOM_TARGETS);
                state->target_re = ZOOM_TARGETS[state->last_zoom_idx].re;
                state->target_im = ZOOM_TARGETS[state->last_zoom_idx].im;
                break;
        }
        state->deep_zoom = state->home_zoom / TOUR_ZOOM_DEPTH;
    }
    
    // Smooth start: if already zoomed out, pan directly. Otherwise, zoom out first.
    if (view->zoom >= 0.1) {
        state->phase = TOUR_PANNING;
    } else {
        state->phase = TOUR_ZOOMING_OUT;
    }
    state->phase_start = 0;
}

void stop_tour(TourState* state) {
    state->phase = TOUR_IDLE;
}

// initializes the julia parameter tour
void start_julia_tour(JuliaTourState* state, complex_t* julia_c, uint32_t now) {
    state->from_re = julia_c->re;
    state->from_im = julia_c->im;
    state->phase = JULIA_TOUR_DWELLING;
    state->phase_start = now;
}

void stop_julia_tour(JuliaTourState* state) {
    state->phase = JULIA_TOUR_IDLE;
}

// advances the julia parameter state machine (keyframe interpolation)
void update_julia_tour(JuliaTourState* state, complex_t* julia_c, uint32_t now) {
    if (state->phase == JULIA_TOUR_IDLE) return;

    if (state->phase == JULIA_TOUR_MOVING) {
        int32_t dt = (int32_t)(now - state->phase_start);
        double raw_t = fmax(0.0, fmin((double)dt / JULIA_TOUR_MOVE_MS, 1.0));
        double e = smoothstep(raw_t);
        julia_c->re = state->from_re + (state->to_re - state->from_re) * e;
        julia_c->im = state->from_im + (state->to_im - state->from_im) * e;
        if (raw_t >= 1.0) {
            julia_c->re = state->to_re;
            julia_c->im = state->to_im;
            state->phase = JULIA_TOUR_DWELLING;
            state->phase_start = now;
        }
    } else {
        // dwell phase: stay at keyframe for a brief moment before moving again
        int32_t dt = (int32_t)(now - state->phase_start);
        if ((double)dt >= JULIA_TOUR_DWELL_MS) {
            state->from_re = julia_c->re;
            state->from_im = julia_c->im;
            state->last_julia_idx = pick_idx(state->last_julia_idx, NUM_JULIA_C_TARGETS);
            state->to_re = JULIA_C_TARGETS[state->last_julia_idx].re;
            state->to_im = JULIA_C_TARGETS[state->last_julia_idx].im;
            state->phase = JULIA_TOUR_MOVING;
            state->phase_start = now;
        }
    }
}

// converts enum state to human readable string for hud display
const char* get_tour_phase_name(TourPhase phase) {
    static const char* PHASE_NAMES[] = {"", "Panning", "Zooming in", "Zooming out"};
    if (phase >= 0 && phase < 4) return PHASE_NAMES[phase];
    return "";
}

int get_tour_target_idx(const TourState* state) {
    if (state->is_dynamic) {
        /* count how many bookmarks of matching fractal type appear before
         * last_zoom_idx to convert the absolute index to a type-relative index */
        int total = get_bookmark_count();
        Bookmark target_b;
        if (!load_bookmark(state->last_zoom_idx, &target_b)) return 0;
        int match_idx = 0;
        for (int i = 0; i < state->last_zoom_idx && i < total; i++) {
            Bookmark b_cur;
            if (load_bookmark(i, &b_cur) && b_cur.fractal_type == target_b.fractal_type) {
                match_idx++;
            }
        }
        return match_idx;
    }
    return state->last_zoom_idx;
}

int get_num_tour_targets(int base_fractal) {
    int total = get_bookmark_count();
    int match_count = 0;
    for (int i = 0; i < total; i++) {
        Bookmark b;
        if (load_bookmark(i, &b)) {
            if (b.fractal_type == base_fractal) {
                match_count++;
            }
        }
    }
    if (match_count > 0) {
        return match_count;
    }

    switch (base_fractal) {
        case RENDER_BURNING_SHIP: return NUM_SHIP_ZOOM_TARGETS;
        case RENDER_TRICORN: return NUM_TRICORN_ZOOM_TARGETS;
        case RENDER_CELTIC: return NUM_CELTIC_ZOOM_TARGETS;
        case RENDER_BUFFALO: return NUM_BUFFALO_ZOOM_TARGETS;
        default: return NUM_ZOOM_TARGETS;
    }
}

// retrieves current target coordinate for hud telemetry
double get_tour_target_re(const TourState* state, int base_fractal) {
    if (state->phase == TOUR_IDLE) return 0.0;
    return state->target_re;
}

double get_tour_target_im(const TourState* state, int base_fractal) {
    if (state->phase == TOUR_IDLE) return 0.0;
    return state->target_im;
}

int get_julia_tour_target_idx(const JuliaTourState* state) {
    return state->last_julia_idx;
}
