/* test_input_handler.c
 *
 * unit tests for the input handler.
 * ensures mouse and keyboard events correctly modify application state.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "input_handler.h"

// test runner macros
#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");

#define EXPECT(cond)                                                        \
    if (!(cond)) {                                                          \
        fprintf(stderr, "\ntest failed: %s at line %d\n", #cond, __LINE__); \
        exit(1);                                                            \
    }

void test_keyboard_iterations(void) {
    TEST_START("keyboard iterations control");
    AppCommonState state;
    app_state_init(&state, 800, 600);

    int initial_iters = state.max_iterations;

    // [TEST CASE] up arrow increases iterations
    AppInputEvent ev = {0};
    ev.type = INPUT_KEY_DOWN;
    ev.key = KEY_UP;
    app_handle_input(&state, &ev, 100);
    EXPECT(state.max_iterations > initial_iters);

    // [TEST CASE] down arrow decreases iterations
    int new_iters = state.max_iterations;
    ev.key = KEY_DOWN;
    app_handle_input(&state, &ev, 200);
    EXPECT(state.max_iterations < new_iters);
    TEST_END();
}

void test_keyboard_actions(void) {
    TEST_START("keyboard actions dispatch");
    AppCommonState state;
    app_state_init(&state, 800, 600);

    // [TEST CASE] quit action
    AppInputEvent ev = {0};
    ev.type = INPUT_KEY_DOWN;
    ev.key = KEY_ESCAPE;
    InputAction action = app_handle_input(&state, &ev, 100);
    EXPECT(action == ACTION_QUIT);

    // [TEST CASE] precision toggle
    ev.key = KEY_E;
    action = app_handle_input(&state, &ev, 200);
    EXPECT(action == ACTION_TOGGLE_PRECISION);
    TEST_END();
}

int main(void) {
    printf("--- input handler tests ---\n");
    test_keyboard_iterations();
    test_keyboard_actions();
    printf("all input handler tests passed!\n");
    return 0;
}
