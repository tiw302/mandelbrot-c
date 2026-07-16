/* test_app_state.c
 *
 * unit tests for the common application state manager.
 * validates state initialization, julia toggling, and boundaries.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "app_state.h"

// mock title callback
static void mock_title_callback(const char* title) {
    (void)title;
}

/* 
 * [TEST CASE] init
 * tests the functionality of init.
 */
int test_init() {
    AppCommonState state = {0};
    app_state_init(&state, 800, 600);

    if (state.max_iterations <= 0) {
        printf("failed: max_iterations should be positive\n");
        return 0;
    }
    if (state.needs_redraw != 1) {
        printf("failed: needs_redraw should be initialized to 1\n");
        return 0;
    }
    if (state.julia_mode != 0) {
        printf("failed: julia_mode should default to 0\n");
        return 0;
    }
    return 1;
}

/* 
 * [TEST CASE] toggle julia
 * tests the functionality of toggle julia.
 */
int test_toggle_julia() {
    AppCommonState state = {0};
    app_state_init(&state, 800, 600);

    app_state_toggle_julia(&state, mock_title_callback);
    if (state.julia_mode != 1) {
        printf("failed: julia_mode not enabled after toggle\n");
        return 0;
    }
    if (state.julia_session.active != 1) {
        printf("failed: julia_session.active not set\n");
        return 0;
    }

    app_state_toggle_julia(&state, mock_title_callback);
    if (state.julia_mode != 0) {
        printf("failed: julia_mode not disabled after toggle\n");
        return 0;
    }
    return 1;
}

/* 
 * [TEST CASE] cycle palette
 * tests the functionality of cycle palette.
 */
int test_cycle_palette() {
    AppCommonState state = {0};
    app_state_init(&state, 800, 600);
    int initial = state.palette_idx;

    app_state_cycle_palette(&state);
    if (state.palette_idx == initial && get_palette_count() > 1) {
        printf("failed: palette_idx did not change after cycling\n");
        return 0;
    }
    return 1;
}

/* 
 * [TEST CASE] boundaries
 * tests the functionality of boundaries.
 */
int test_boundaries() {
    AppCommonState state = {0};
    app_state_init(&state, 800, 600);

    precise_float re_min, re_max, im_min, im_max;
    app_state_calculate_boundaries(&state, 800, 600, &re_min, &re_max, &im_min, &im_max);

    if (re_min >= re_max || im_min >= im_max) {
        printf("failed: invalid boundaries calculated\n");
        return 0;
    }
    return 1;
}

int main() {
    int passed = 0;
    int failed = 0;

    printf("running app state manager tests...\n");

    if (test_init()) {
        printf("  [pass] test_init\n");
        passed++;
    } else {
        printf("  [FAIL] test_init\n");
        failed++;
    }

    if (test_toggle_julia()) {
        printf("  [pass] test_toggle_julia\n");
        passed++;
    } else {
        printf("  [FAIL] test_toggle_julia\n");
        failed++;
    }

    if (test_cycle_palette()) {
        printf("  [pass] test_cycle_palette\n");
        passed++;
    } else {
        printf("  [FAIL] test_cycle_palette\n");
        failed++;
    }

    if (test_boundaries()) {
        printf("  [pass] test_boundaries\n");
        passed++;
    } else {
        printf("  [FAIL] test_boundaries\n");
        failed++;
    }

    printf("\nresults: %d passed, %d failed\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
