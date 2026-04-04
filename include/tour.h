#ifndef TOUR_H
#define TOUR_H

#include <SDL2/SDL.h>
#include "renderer.h"

typedef enum {
    TOUR_IDLE = 0,
    TOUR_PANNING,
    TOUR_ZOOMING_IN,
    TOUR_ZOOMING_OUT
} TourPhase;

typedef enum {
    JULIA_TOUR_IDLE = 0,
    JULIA_TOUR_MOVING,
    JULIA_TOUR_DWELLING
} JuliaTourPhase;

typedef struct {
    double center_re;
    double center_im;
    double zoom;
} ViewState;

typedef struct {
    TourPhase phase;
    double    home_re, home_im, home_zoom;
    double    target_re, target_im, deep_zoom;
    Uint32    phase_start;
    int       last_zoom_idx;
} TourState;

typedef struct {
    JuliaTourPhase phase;
    double         from_re, from_im;
    double         to_re, to_im;
    Uint32         phase_start;
    int            last_julia_idx;
} JuliaTourState;

void update_tour(TourState *state, ViewState *view, Uint32 now);
void update_julia_tour(JuliaTourState *state, complex_t *julia_c, Uint32 now);

const char *get_tour_phase_name(TourPhase phase);
int get_tour_target_idx(const TourState *state);
int get_julia_tour_target_idx(const JuliaTourState *state);

#endif
