/* tour.c
 *
 * implements automatic animated tours across pre-selected interesting coordinates.
 * handles state transition, interpolation (easing), and camera panning/zooming.
 */

#include "tour.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "config.h"

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

// standard hermite interpolation for smooth ease-in/out motion
static inline double smoothstep(double t) {
    return t * t * (3.0 - 2.0 * t);
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

// advances the main fractal tour state machine based on current timestamp
void update_tour(TourState* state, ViewState* view, uint32_t now, int is_burning_ship) {
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
        case TOUR_PANNING:
            // linear interpolation for camera position during panning
            view->center_re = state->home_re + (state->target_re - state->home_re) * e;
            view->center_im = state->home_im + (state->target_im - state->home_im) * e;
            view->zoom = state->home_zoom;
            if (raw_t >= 1.0) {
                view->center_re = state->target_re;
                view->center_im = state->target_im;
                state->phase = TOUR_ZOOMING_IN;
                state->phase_start = now;
            }
            break;
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
        case TOUR_ZOOMING_OUT:
            // return to home position while pulling back the camera
            view->center_re = state->target_re + (state->home_re - state->target_re) * e;
            view->center_im = state->target_im + (state->home_im - state->target_im) * e;
            view->zoom =
                exp(log(state->deep_zoom) + (log(state->home_zoom) - log(state->deep_zoom)) * e);
            if (raw_t >= 1.0) {
                view->center_re = state->home_re;
                view->center_im = state->home_im;
                view->zoom = state->home_zoom;

                // select a new random target for the next loop
                if (is_burning_ship) {
                    state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_SHIP_ZOOM_TARGETS);
                    state->target_re = SHIP_ZOOM_TARGETS[state->last_zoom_idx].re;
                    state->target_im = SHIP_ZOOM_TARGETS[state->last_zoom_idx].im;
                } else {
                    state->last_zoom_idx = pick_idx(state->last_zoom_idx, NUM_ZOOM_TARGETS);
                    state->target_re = ZOOM_TARGETS[state->last_zoom_idx].re;
                    state->target_im = ZOOM_TARGETS[state->last_zoom_idx].im;
                }
                state->phase = TOUR_PANNING;
                state->phase_start = now;
            }
            break;
        default:
            break;
    }
}

// initializes tour state based on the current viewport
void start_tour(TourState* state, ViewState* view) {
    state->home_re = view->center_re;
    state->home_im = view->center_im;
    state->home_zoom = view->zoom;
    state->deep_zoom = view->zoom / TOUR_ZOOM_DEPTH;
    state->target_re = view->center_re;
    state->target_im = view->center_im;
    state->phase = TOUR_ZOOMING_OUT;  // start with a reset move
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
    return state->last_zoom_idx;
}

int get_num_tour_targets(int is_burning_ship) {
    return is_burning_ship ? NUM_SHIP_ZOOM_TARGETS : NUM_ZOOM_TARGETS;
}

// retrieves current target coordinate for hud telemetry
double get_tour_target_re(const TourState* state, int is_burning_ship) {
    if (state->phase == TOUR_IDLE) return 0.0;
    if (is_burning_ship) {
        if (state->last_zoom_idx >= 0 && state->last_zoom_idx < NUM_SHIP_ZOOM_TARGETS)
            return SHIP_ZOOM_TARGETS[state->last_zoom_idx].re;
    } else {
        if (state->last_zoom_idx >= 0 && state->last_zoom_idx < NUM_ZOOM_TARGETS)
            return ZOOM_TARGETS[state->last_zoom_idx].re;
    }
    return 0.0;
}

double get_tour_target_im(const TourState* state, int is_burning_ship) {
    if (state->phase == TOUR_IDLE) return 0.0;
    if (is_burning_ship) {
        if (state->last_zoom_idx >= 0 && state->last_zoom_idx < NUM_SHIP_ZOOM_TARGETS)
            return SHIP_ZOOM_TARGETS[state->last_zoom_idx].im;
    } else {
        if (state->last_zoom_idx >= 0 && state->last_zoom_idx < NUM_ZOOM_TARGETS)
            return ZOOM_TARGETS[state->last_zoom_idx].im;
    }
    return 0.0;
}

int get_julia_tour_target_idx(const JuliaTourState* state) {
    return state->last_julia_idx;
}
