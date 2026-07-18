/* test_bookmark.c
 *
 * unit tests for bookmark serialization and file i/o.
 * validates JSON storage precision and boundary values.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bookmark.h"

#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");
#define EXPECT(cond)                                                          \
    if (!(cond)) {                                                            \
        fprintf(stderr, "\\ntest failed: %s at line %d\\n", #cond, __LINE__); \
        exit(1);                                                              \
    }

static int approx_eq(double a, double b) {
    return fabs(a - b) < 1e-12;  // high precision check for bookmarks
}

/*
 * [TEST CASE] bookmark io
 * tests the functionality of bookmark io.
 */
void test_bookmark_io(void) {
    TEST_START("bookmark read/write accuracy");

    Bookmark b_out = {.center_re = -0.743643887037151,
                      .center_im = 0.131825904205330,
                      .zoom = 1.23456789e-10,
                      .max_iterations = 2500,
                      .fractal_type = 1,
                      .julia_c = {-0.8, 0.156}};

    // redirect bookmarks to a temp file for isolated testing
    const char* temp_file = "test_bookmarks.json";
    set_bookmarks_file(temp_file);

    // save to temp file
    save_bookmark(&b_out);

    // check how many we have (should be > 0)
    int count = get_bookmark_count();
    EXPECT(count > 0);

    // load the most recently saved one (last in list)
    Bookmark b_in;
    int success = load_bookmark(count - 1, &b_in);
    EXPECT(success == 1);

    // validate extreme precision is maintained through json conversion
    EXPECT(approx_eq(b_out.center_re, b_in.center_re));
    EXPECT(approx_eq(b_out.center_im, b_in.center_im));
    EXPECT(approx_eq(b_out.zoom, b_in.zoom));
    EXPECT(b_out.max_iterations == b_in.max_iterations);
    EXPECT(b_out.fractal_type == b_in.fractal_type);
    EXPECT(approx_eq(b_out.julia_c.re, b_in.julia_c.re));
    EXPECT(approx_eq(b_out.julia_c.im, b_in.julia_c.im));

    // clean up temporary test file
    remove(temp_file);

    TEST_END();
}

int main(void) {
    printf("--- starting bookmark tests ---\\n");
    // make sure we work in a temp environment or just accept appending to bookmarks.json
    test_bookmark_io();
    printf("--- bookmark tests passed! ---\\n");
    return 0;
}
