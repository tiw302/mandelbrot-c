#include "tour.h"
#include "config.h"
#include <math.h>
#include <stdlib.h>

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

#define JULIA_TOUR_MOVE_MS   3000.0
#define JULIA_TOUR_DWELL_MS  1200.0

static const struct { double re, im; } JULIA_C_TARGETS[] = {
    {-0.7000,  0.2700},  // classic spiral
    {-0.4000,  0.6000},  // rabbit
    { 0.2850,  0.0100},  // coral
    {-0.7269,  0.1889},  // siel disk
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

static inline double smoothstep(double t) { return t * t * (3.0 - 2.0 * t); }

static int pick_idx(int last, int count) {
    int idx;
    do { idx = rand() % count; } while (idx == last && count > 1);
    return idx;
}

void update_tour(TourState *state, ViewState *view, Uint32 now) {
    if (state->phase == TOUR_IDLE) return;

    double duration = (state->phase == TOUR_PANNING)   ? TOUR_PAN_MS :
                      (state->phase == TOUR_ZOOMING_IN) ? TOUR_ZOOM_IN_MS :
                                                        TOUR_ZOOM_OUT_MS;
    double raw_t = fmin((double)(now - state->phase_start) / duration, 1.0);
    double e     = smoothstep(raw_t);

    switch (state->phase) {
    case TOUR_PANNING:
        view->center_re = state->home_re + (state->target_re - state->home_re) * e;
        view->center_im = state->home_im + (state->target_im - state->home_im) * e;
        view->zoom      = state->home_zoom;
        if (raw_t >= 1.0) {
            view->center_re   = state->target_re;
            view->center_im   = state->target_im;
            state->phase       = TOUR_ZOOMING_IN;
            state->phase_start = now;
        }
        break;
    case TOUR_ZOOMING_IN:
        view->center_re = state->target_re;
        view->center_im = state->target_im;
        view->zoom = exp(log(state->home_zoom) +
                        (log(state->deep_zoom) - log(state->home_zoom)) * e);
        if (raw_t >= 1.0) {
            view->zoom        = state->deep_zoom;
            state->phase       = TOUR_ZOOMING_OUT;
            state->phase_start = now;
        }
        break;
    case TOUR_ZOOMING_OUT:
        view->center_re = state->target_re + (state->home_re - state->target_re) * e;
        view->center_im = state->target_im + (state->home_im - state->target_im) * e;
        view->zoom = exp(log(state->deep_zoom) +
                        (log(state->home_zoom) - log(state->deep_zoom)) * e);
        if (raw_t >= 1.0) {
            view->center_re   = state->home_re;
            view->center_im   = state->home_im;
            view->zoom        = state->home_zoom;
            state->last_zoom_idx    = pick_idx(state->last_zoom_idx, NUM_ZOOM_TARGETS);
            state->target_re   = ZOOM_TARGETS[state->last_zoom_idx].re;
            state->target_im   = ZOOM_TARGETS[state->last_zoom_idx].im;
            state->phase       = TOUR_PANNING;
            state->phase_start = now;
        }
        break;
    default: break;
    }
}

void update_julia_tour(JuliaTourState *state, complex_t *julia_c, Uint32 now) {
    if (state->phase == JULIA_TOUR_IDLE) return;

    if (state->phase == JULIA_TOUR_MOVING) {
        double raw_t = fmin((double)(now - state->phase_start) / JULIA_TOUR_MOVE_MS, 1.0);
        double e     = smoothstep(raw_t);
        julia_c->re = state->from_re + (state->to_re - state->from_re) * e;
        julia_c->im = state->from_im + (state->to_im - state->from_im) * e;
        if (raw_t >= 1.0) {
            julia_c->re     = state->to_re;
            julia_c->im     = state->to_im;
            state->phase     = JULIA_TOUR_DWELLING;
            state->phase_start = now;
        }
    } else { // JULIA_TOUR_DWELLING
        if ((double)(now - state->phase_start) >= JULIA_TOUR_DWELL_MS) {
            state->from_re = julia_c->re;
            state->from_im = julia_c->im;
            state->last_julia_idx = pick_idx(state->last_julia_idx, NUM_JULIA_C_TARGETS);
            state->to_re   = JULIA_C_TARGETS[state->last_julia_idx].re;
            state->to_im   = JULIA_C_TARGETS[state->last_julia_idx].im;
            state->phase   = JULIA_TOUR_MOVING;
            state->phase_start = now;
        }
    }
}

const char *get_tour_phase_name(TourPhase phase) {
    static const char *PHASE_NAMES[] = { "", "Panning", "Zooming in", "Zooming out" };
    if (phase >= 0 && phase < 4) return PHASE_NAMES[phase];
    return "";
}

int get_tour_target_idx(const TourState *state) {
    return state->last_zoom_idx;
}

int get_julia_tour_target_idx(const JuliaTourState *state) {
    return state->last_julia_idx;
}
