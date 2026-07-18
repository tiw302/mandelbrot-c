/* test_camera.c
 *
 * unit tests for the camera and viewport system.
 * verifies coordinate translation, pan, zoom limits, and history buffer.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "camera.h"

// test runner macros
#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");

#define EXPECT(cond)                                                        \
    if (!(cond)) {                                                          \
        fprintf(stderr, "\ntest failed: %s at line %d\n", #cond, __LINE__); \
        exit(1);                                                            \
    }

#define EXPECT_NEAR(a, b, tol) EXPECT(fabs((double)((a) - (b))) <= (tol))

void test_camera_init(void) {
    TEST_START("camera initialization");
    Camera cam;
    camera_init(&cam, 800, 600);

    // [TEST CASE] camera initialized to default mandelbrot view
    EXPECT(cam.win_w == 800);
    EXPECT(cam.win_h == 600);
    EXPECT_NEAR(cam.view.center_re, -0.5, 1e-6);
    EXPECT_NEAR(cam.view.center_im, 0.0, 1e-6);
    EXPECT(cam.history_count == 0);
    TEST_END();
}

void test_camera_coordinate_translation(void) {
    TEST_START("camera screen to complex");
    Camera cam;
    camera_init(&cam, 800, 800);  // square aspect ratio
    cam.view.center_re = 0.0;
    cam.view.center_im = 0.0;
    cam.view.zoom = 1.0;

    precise_float re, im;

    // [TEST CASE] center of screen translates to center coordinates
    camera_screen_to_complex(&cam, 400, 400, &re, &im);
    EXPECT_NEAR(re, 0.0, 1e-6);
    EXPECT_NEAR(im, 0.0, 1e-6);

    // [TEST CASE] left edge of screen
    camera_screen_to_complex(&cam, 0, 400, &re, &im);
    EXPECT_NEAR(re, -0.5, 1e-6);

    // [TEST CASE] top edge of screen
    camera_screen_to_complex(&cam, 400, 0, &re, &im);
    EXPECT_NEAR(im, 0.5, 1e-6);
    TEST_END();
}

void test_camera_history(void) {
    TEST_START("camera history stack");
    Camera cam;
    camera_init(&cam, 800, 600);

    // [TEST CASE] history push
    cam.view.zoom = 1.0;
    camera_push_history(&cam);
    EXPECT(cam.history_count == 1);

    // [TEST CASE] history pop
    cam.view.zoom = 0.1;
    bool changed = camera_pop_history(&cam);
    EXPECT(changed == true);
    EXPECT(cam.history_count == 0);
    EXPECT_NEAR(cam.view.zoom, 1.0, 1e-6);

    // [TEST CASE] pop empty history
    changed = camera_pop_history(&cam);
    EXPECT(changed == false);
    TEST_END();
}

int main(void) {
    printf("--- camera tests ---\n");
    test_camera_init();
    test_camera_coordinate_translation();
    test_camera_history();
    printf("all camera tests passed!\n");
    return 0;
}
