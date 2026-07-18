/* benchmark_bignum.c
 *
 * performance benchmarking for arbitrary-precision bignum mathematics.
 * measures calculation speed of mandelbrot iterations at extreme zoom depths.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bignum.h"
#include "mandelbrot_bignum.h"

#define MAX_ITERATIONS 1000

// returns time in seconds
double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    printf("==========================================\n");
    printf(" BigNum Precision Benchmark               \n");
    printf(" Max Iterations: %d                       \n", MAX_ITERATIONS);
    printf("==========================================\n\n");

    BigNum c_re, c_im;
    bn_from_double(&c_re, -0.743643887037151);
    bn_from_double(&c_im, 0.131825904205330);

    int num_runs = 10;
    printf("Computing BigNum reference orbit %d times...\n", num_runs);

    double start = get_time_sec();

    int total_len = 0;
    double* orbit_re = malloc(sizeof(double) * MAX_ITERATIONS);
    double* orbit_im = malloc(sizeof(double) * MAX_ITERATIONS);

    if (!orbit_re || !orbit_im) {
        printf("memory allocation failed\n");
        return 1;
    }

    for (int i = 0; i < num_runs; i++) {
        total_len += mandelbrot_bignum_orbit(&c_re, &c_im, MAX_ITERATIONS, orbit_re, orbit_im);
    }

    double end = get_time_sec();
    double time_taken = end - start;

    printf("Total time: %.4f seconds\n", time_taken);
    printf("Average time per BigNum orbit: %.4f seconds\n", time_taken / num_runs);
    printf("Average orbit length: %d\n", total_len / num_runs);

    free(orbit_re);
    free(orbit_im);

    printf("Benchmark complete.\n");

    return 0;
}
