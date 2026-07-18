/* test_fractal.c
 *
 * unit tests for fractal equations.
 * ensures that burning ship, tricorn, celtic, and buffalo are mathematically correct.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffalo.h"
#include "burning_ship.h"
#include "celtic.h"
#include "tricorn.h"

// test runner macros
#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");

#define EXPECT(cond)                                                        \
    if (!(cond)) {                                                          \
        fprintf(stderr, "\ntest failed: %s at line %d\n", #cond, __LINE__); \
        exit(1);                                                            \
    }

// tolerance for floating point comparisons
#define EXPECT_NEAR(a, b, tol) EXPECT(fabs((a) - (b)) <= (tol))

void test_burning_ship(void) {
    TEST_START("burning_ship");
    // [TEST CASE] origin should never escape
    complex_t origin = {0.0, 0.0};
    EXPECT(burning_ship_check(origin, 100) == 100);

    // [TEST CASE] outside point should escape quickly
    complex_t outside = {2.0, 2.0};
    double iters = burning_ship_check(outside, 100);
    EXPECT(iters < 10);
    TEST_END();
}

void test_tricorn(void) {
    TEST_START("tricorn");
    // [TEST CASE] origin should never escape
    complex_t origin = {0.0, 0.0};
    EXPECT(tricorn_check(origin, 100) == 100);

    // [TEST CASE] point on real axis
    complex_t real_point = {0.25, 0.0};
    EXPECT(tricorn_check(real_point, 100) == 100);

    // [TEST CASE] outside point should escape
    complex_t outside = {2.0, 2.0};
    double iters = tricorn_check(outside, 100);
    EXPECT(iters < 10);
    TEST_END();
}

void test_celtic(void) {
    TEST_START("celtic");
    // [TEST CASE] origin should never escape
    complex_t origin = {0.0, 0.0};
    EXPECT(celtic_check(origin, 100) == 100);

    // [TEST CASE] outside point
    complex_t outside = {2.0, 2.0};
    double iters = celtic_check(outside, 100);
    EXPECT(iters < 10);
    TEST_END();
}

void test_buffalo(void) {
    TEST_START("buffalo");
    // [TEST CASE] origin should never escape
    complex_t origin = {0.0, 0.0};
    EXPECT(buffalo_check(origin, 100) == 100);

    // [TEST CASE] outside point
    complex_t outside = {2.0, 2.0};
    double iters = buffalo_check(outside, 100);
    EXPECT(iters < 10);
    TEST_END();
}

int main(void) {
    printf("--- fractal tests ---\n");
    test_burning_ship();
    test_tricorn();
    test_celtic();
    test_buffalo();
    printf("all fractal tests passed!\n");
    return 0;
}
