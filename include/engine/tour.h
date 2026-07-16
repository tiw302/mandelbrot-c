#ifndef TOUR_H
#define TOUR_H

#include <stdint.h>

#include "renderer.h"

// Tour default durations
#define TOUR_ZOOM_DEPTH 6000.0
#define TOUR_PAN_MS 1800.0
#define TOUR_ZOOM_IN_MS 4000.0
#define TOUR_ZOOM_OUT_MS 3200.0

#define JULIA_TOUR_MOVE_MS 3000.0
#define JULIA_TOUR_DWELL_MS 1200.0

// tour phase enumeration for the mandelbrot/burning ship tour
typedef enum {
    TOUR_IDLE = 0,
    TOUR_PANNING,     // moving the camera to the next target
    TOUR_ZOOMING_IN,  // performing a deep zoom into the fractal
    TOUR_ZOOMING_OUT  // pulling back to the original view
} TourPhase;

// phase enumeration for the julia set parameter tour
typedef enum {
    JULIA_TOUR_IDLE = 0,
    JULIA_TOUR_MOVING,   // interpolating between two c-parameters
    JULIA_TOUR_DWELLING  // pausing at a specific c-parameter coordinate
} JuliaTourPhase;

// state tracking for the main fractal tour (mandelbrot/burning ship)
typedef struct {
    TourPhase phase;
    double home_re, home_im, home_zoom;      // the starting view to return to
    double target_re, target_im, deep_zoom;  // the point of interest to zoom into
    uint32_t phase_start;                    // timestamp of current phase beginning
    int last_zoom_idx;                       // index of the target in the target array
    int is_dynamic;                          // 1 if using bookmarks, 0 if using presets
    double pan_ms;                           // duration of the initial pan movement
    double zoom_in_ms;                       // duration of the deep zoom phase
    double zoom_out_ms;                      // duration of the pull-back phase
    int zoom_curve;                          // 0=Ease-In-Out, 1=Linear, 2=Ease-In, 3=Ease-Out
} TourState;

// state tracking for the julia parameter tour
typedef struct {
    JuliaTourPhase phase;
    double from_re, from_im;  // starting c-parameter
    double to_re, to_im;      // target c-parameter
    uint32_t phase_start;     // timestamp of current phase beginning
    int last_julia_idx;       // index of current keyframe
    double move_ms;           // duration of parameter interpolation
    double dwell_ms;          // duration of pause at keyframes
} JuliaTourState;

// core update logic — called once per frame to advance animations
void update_tour(TourState* state, ViewState* view, uint32_t now, int base_fractal);
void update_julia_tour(JuliaTourState* state, complex_t* julia_c, uint32_t now);

// lifecycle management for mandelbrot tours
void start_tour(TourState* state, ViewState* view, int base_fractal);
void stop_tour(TourState* state);

// lifecycle management for julia parameter tours
void start_julia_tour(JuliaTourState* state, complex_t* julia_c, uint32_t now);
void stop_julia_tour(JuliaTourState* state);

// utility helpers for ui feedback and state inspection
const char* get_tour_phase_name(TourPhase phase);
int get_tour_target_idx(const TourState* state);
int get_num_tour_targets(int base_fractal);
double get_tour_target_re(const TourState* state, int base_fractal);
double get_tour_target_im(const TourState* state, int base_fractal);
int get_julia_tour_target_idx(const JuliaTourState* state);

#endif
