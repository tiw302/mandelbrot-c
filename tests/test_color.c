// test_color.c — unit test for color palette initialization and interpolation.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "color.h"
#include "config.h"

#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");
#define EXPECT(cond)                                                          \
    if (!(cond)) {                                                            \
        fprintf(stderr, "\\ntest failed: %s at line %d\\n", #cond, __LINE__); \
        exit(1);                                                              \
    }

void test_palette_bounds(void) {
    TEST_START("color palette bounds checking");

    uint8_t r, g, b;
    int max_iters = 1000;

    // initialize standard palettes
    init_color_palette(max_iters, 0);

    // test across all available palettes to ensure no index out-of-bounds
    // and that rgb values are always clamped between 0 and 255.
    for (int p = 0; p < get_palette_count(); p++) {
        init_color_palette(max_iters, p);

        // test various iteration values: 0, max, and fractional
        double test_iters[] = {0.0, 50.5, 999.9, 1000.0, 1005.0, -1.0};

        for (int i = 0; i < 6; i++) {
            get_color(test_iters[i], max_iters, &r, &g, &b);
            // we can't strictly check max value since uint8_t caps at 255 anyway,
            // but we can check if it crashes or produces wildly unexpected behavior.
            // if it escaped max_iters it should be black.
            if (test_iters[i] >= max_iters) {
                EXPECT(r == 0 && g == 0 && b == 0);
            }
        }
    }
    TEST_END();
}

void test_color_smoothness(void) {
    TEST_START("color smooth interpolation");

    uint8_t r1, g1, b1;
    uint8_t r2, g2, b2;
    uint8_t r_mid, g_mid, b_mid;
    int max_iters = 100;

    init_color_palette(max_iters, 1);  // use grayscale for predictable linearity

    get_color(10.0, max_iters, &r1, &g1, &b1);
    get_color(11.0, max_iters, &r2, &g2, &b2);
    get_color(10.5, max_iters, &r_mid, &g_mid, &b_mid);

    // the midpoint color should be roughly halfway between the two integer iterations
    int mid_expected_r = (r1 + r2) / 2;
    EXPECT(abs(r_mid - mid_expected_r) <= 2);

    TEST_END();
}

int main(void) {
    printf("--- starting color tests ---\\n");
    test_palette_bounds();
    test_color_smoothness();
    printf("--- color tests passed! ---\\n");
    return 0;
}
