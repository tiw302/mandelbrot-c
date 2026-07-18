/* benchmark_fractal_types.c
 *
 * performance benchmarking comparing different fractal equations.
 * evaluates branch prediction and absolute value overhead in non-mandelbrot fractals
 * like burning ship, tricorn, celtic, and buffalo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core_math.h"
#include "burning_ship.h"
#include "tricorn.h"
#include "celtic.h"
#include "buffalo.h"

#define MAX_ITERATIONS 1000
#define GRID_WIDTH 1024
#define GRID_HEIGHT 1024

// returns time in seconds
double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// function pointer type for fractal checks
typedef double (*fractal_check_func)(complex_t c, int max_iterations);

void benchmark_fractal(const char* name, fractal_check_func func) {
    printf(" Benchmarking %-15s... ", name);
    fflush(stdout);

    // use a predefined viewport (-2.0, -1.5) to (1.0, 1.5)
    double re_min = -2.0;
    double re_max = 1.0;
    double im_min = -1.5;
    double im_max = 1.5;
    
    double dx = (re_max - re_min) / GRID_WIDTH;
    double dy = (im_max - im_min) / GRID_HEIGHT;

    double start_time = get_time_sec();
    
    // volatile to prevent the compiler from optimizing away the loop
    volatile double dummy_sum = 0.0;

    for (int y = 0; y < GRID_HEIGHT; y++) {
        double im = im_min + y * dy;
        for (int x = 0; x < GRID_WIDTH; x++) {
            double re = re_min + x * dx;
            complex_t c = {re, im};
            dummy_sum += func(c, MAX_ITERATIONS);
        }
    }

    double end_time = get_time_sec();
    double duration = end_time - start_time;
    
    printf("%6.3f seconds (sum: %f)\n", duration, dummy_sum);
}

int main(void) {
    printf("==========================================\n");
    printf(" Fractal Types Benchmark                  \n");
    printf(" Resolution: %d x %d (%.2f Million Pixels)\n", GRID_WIDTH, GRID_HEIGHT,
           (GRID_WIDTH * GRID_HEIGHT) / 1e6);
    printf(" Max Iterations: %d                       \n", MAX_ITERATIONS);
    printf("==========================================\n\n");

    // run benchmarks
    benchmark_fractal("Mandelbrot", mandelbrot_check);
    benchmark_fractal("Burning Ship", burning_ship_check);
    benchmark_fractal("Tricorn", tricorn_check);
    benchmark_fractal("Celtic", celtic_check);
    benchmark_fractal("Buffalo", buffalo_check);

    printf("\n==========================================\n");
    return 0;
}
