/* test_tour.c
 *
 * unit tests for the cinematic tour state machine.
 * validates phase transitions and target viewport interpolation.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "tour.h"

/*
 * [TEST CASE] tour transitions
 * tests the functionality of tour transitions.
 */
int test_tour_transitions() {
    TourState state = {0};
    ViewState view = {0.0, 0.0, 0.01};

    start_tour(&state, &view, 0);
    if (state.phase != TOUR_ZOOMING_OUT) {
        printf("failed: initial phase should be TOUR_ZOOMING_OUT (got %d)\n", state.phase);
        return 0;
    }

    /*
     * [TEST CASE] time initialization
     * first update sets the baseline time (phase_start).
     * the delta time here should be effectively zeroed out.
     */
    uint32_t start_time = 1000;
    update_tour(&state, &view, start_time, 0, 1.0);  // initial
    if (state.phase_start != start_time) {
        printf("failed: phase_start not set correctly\n");
        return 0;
    }

    /*
     * [TEST CASE] negative delta / time skew resilience
     * simulating backward time shifts. this happens if the system clock
     * jitters or if the game loop sends a stale timestamp. the state machine
     * must clamp 'raw_t' to 0 and avoid transitioning states unexpectedly.
     */
    update_tour(&state, &view, start_time - 500, 0, 1.0);
    // should still be zooming out safely
    if (state.phase != TOUR_ZOOMING_OUT) {
        printf("failed: phase changed unexpectedly when time went backwards\n");
        return 0;
    }

    // fast forward to end of zoom out
    update_tour(&state, &view, start_time + 4000, 0, 1.0);  // zoom_out is 3200ms
    if (state.phase != TOUR_PANNING) {
        printf("failed: did not transition to TOUR_PANNING\n");
        return 0;
    }

    // verify panning target selected
    int next_idx = get_tour_target_idx(&state);
    if (next_idx < 0 || next_idx >= get_num_tour_targets(0)) {
        printf("failed: invalid target idx selected\n");
        return 0;
    }

    return 1;
}

/*
 * [TEST CASE] julia tour
 * tests the functionality of julia tour.
 */
int test_julia_tour() {
    JuliaTourState state = {0};
    complex_t julia_c = {0.0, 0.0};

    start_julia_tour(&state, &julia_c, 1000);
    if (state.phase != JULIA_TOUR_DWELLING) {
        printf("failed: initial julia phase should be DWELLING\n");
        return 0;
    }

    // dwell time is 1200ms
    update_julia_tour(&state, &julia_c, 1500, 1.0);  // 500ms later
    if (state.phase != JULIA_TOUR_DWELLING) {
        printf("failed: julia phase should still be DWELLING\n");
        return 0;
    }

    update_julia_tour(&state, &julia_c, 2300, 1.0);  // 1300ms later
    if (state.phase != JULIA_TOUR_MOVING) {
        printf("failed: julia phase should have transitioned to MOVING\n");
        return 0;
    }

    return 1;
}

int main() {
    int passed = 0;
    int failed = 0;

    printf("running tour state machine tests...\n");

    if (test_tour_transitions()) {
        printf("  [pass] test_tour_transitions\n");
        passed++;
    } else {
        printf("  [FAIL] test_tour_transitions\n");
        failed++;
    }

    if (test_julia_tour()) {
        printf("  [pass] test_julia_tour\n");
        passed++;
    } else {
        printf("  [FAIL] test_julia_tour\n");
        failed++;
    }

    printf("\nresults: %d passed, %d failed\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
