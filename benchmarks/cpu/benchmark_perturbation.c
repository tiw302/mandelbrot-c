/* benchmark_perturbation.c
 *
 * performance benchmarking for cpu perturbation theory reference orbit calculation.
 * measures how fast the cpu can compute the reference path and series approximation coefficients.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "perturbation.h"

#define MAX_ITERATIONS 10000

double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    printf("==========================================\n");
    printf(" Perturbation Theory Benchmark            \n");
    printf(" Max Iterations: %d                       \n", MAX_ITERATIONS);
    printf("==========================================\n\n");

    precise_float center_re = -0.743643887037151;
    precise_float center_im = 0.131825904205330;

    int num_runs = 100;
    
    printf("Computing reference orbit and SA coefficients %d times...\n", num_runs);
    double start = get_time_sec();

    long total_orbit_len = 0;
    for (int i = 0; i < num_runs; i++) {
        RefOrbit* orbit = perturbation_compute(center_re, center_im, MAX_ITERATIONS);
        if (orbit) {
            total_orbit_len += orbit->len;
            perturbation_free(orbit);
        }
    }

    double end = get_time_sec();
    double time_taken = end - start;

    printf("Total time: %.4f seconds\n", time_taken);
    printf("Average time per RefOrbit: %.4f ms\n", (time_taken * 1000.0) / num_runs);
    printf("Average orbit length: %ld\n", total_orbit_len / num_runs);
    printf("Benchmark complete.\n");

    return 0;
}
