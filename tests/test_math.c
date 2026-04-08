#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mandelbrot.h"
#include "julia.h"
#include "config.h"

/* simple test runner macros */
#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");

#define EXPECT(cond) if (!(cond)) { \
    fprintf(stderr, "\nTest failed: %s at line %d\n", #cond, __LINE__); \
    exit(1); \
}

/* helper to compare doubles with epsilon */
int approx_eq(double a, double b) {
    return fabs(a - b) < 1e-7;
}

void test_mandelbrot_basics() {
    TEST_START("mandelbrot_check basics");
    
    // point inside the set (main cardioid)
    complex_t c_inside = {0.0, 0.0};
    EXPECT(mandelbrot_check(c_inside, 100) == 100.0);
    
    // point outside the set
    complex_t c_outside = {2.0, 2.0};
    EXPECT(mandelbrot_check(c_outside, 100) < 100.0);
    
    // point that escapes immediately (outside escape radius)
    complex_t c_immediate = {ESCAPE_RADIUS + 1.0, 0.0};
    EXPECT(mandelbrot_check(c_immediate, 100) < 100.0);

    // point in period-2 bulb
    complex_t c_bulb = {-1.0, 0.0};
    EXPECT(mandelbrot_check(c_bulb, 100) == 100.0);
    
    TEST_END();
}

void test_julia_basics() {
    TEST_START("julia_check basics");
    
    // standard julia set point (circle at c=0, z=0.5)
    complex_t z = {0.5, 0.0};
    complex_t c_julia = {0.0, 0.0};
    EXPECT(julia_check(z, c_julia, 100) == 100.0);
    
    // point way outside
    complex_t z_out = {2.0, 2.0};
    EXPECT(julia_check(z_out, c_julia, 100) < 100.0);
    
    TEST_END();
}

#ifdef __AVX2__
void test_avx2_consistency() {
    TEST_START("avx2 vs scalar consistency");
    
    // include points that escape at different stages
    double re[4] = {0.0, 2.0, 0.2, 11.0};
    double im[4] = {0.0, 2.0, -0.2, 0.0};
    double results_avx[4];
    int max_iters = 100;
    
    // test mandelbrot avx
    mandelbrot_check_avx2(re, im, max_iters, results_avx);
    for(int i = 0; i < 4; i++) {
        complex_t c = {re[i], im[i]};
        double res_scalar = mandelbrot_check(c, max_iters);
        EXPECT(approx_eq(results_avx[i], res_scalar));
    }
    
    // test julia avx
    complex_t c_julia = {-0.8, 0.156};
    julia_check_avx2(re, im, c_julia, max_iters, results_avx);
    for(int i = 0; i < 4; i++) {
        complex_t z = {re[i], im[i]};
        double res_scalar = julia_check(z, c_julia, max_iters);
        EXPECT(approx_eq(results_avx[i], res_scalar));
    }
    
    TEST_END();
}
#endif

int main() {
    printf("--- starting core math tests ---\n");
    
    test_mandelbrot_basics();
    test_julia_basics();
    
#ifdef __AVX2__
    test_avx2_consistency();
#else
    printf("skipped avx2 tests (not compiled with avx2)\n");
#endif

    printf("--- all tests passed! ---\n");
    return 0;
}
