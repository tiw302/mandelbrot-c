// benchmark_renderer.c — measures multi-threaded renderer performance.
// tests thread pool load balancing and pixel memory write throughput.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "renderer.h"

#define MAX_ITERATIONS 1000
#define NUM_RUNS 10

// returns time in seconds
static double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void run_benchmark(const char* name, int width, int height) {
    printf("==========================================\n");
    printf(" %s\n", name);
    printf(" Resolution: %d x %d (%.2f Million Pixels)\n", width, height, (width * height) / 1e6);
    printf(" Max Iterations: %d\n", MAX_ITERATIONS);
    printf(" Thread Count: %d\n", get_actual_thread_count());
    printf("==========================================\n\n");

    uint32_t* pixels = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    if (!pixels) {
        printf("error: memory allocation failed for %s\n", name);
        return;
    }

    const double x_min = -2.0;
    const double x_max = 1.0;
    const double y_min = -1.5;
    const double y_max = 1.5;

    double total_time = 0.0;
    long long total_pixels = (long long)width * height;

    printf("Running %d frames...\n", NUM_RUNS);

    // warm up run
    render_mandelbrot_threaded(pixels, width * 4, width, height, x_min, x_max, y_min, y_max,
                               MAX_ITERATIONS);

    for (int i = 0; i < NUM_RUNS; i++) {
        double start = get_time_sec();
        render_mandelbrot_threaded(pixels, width * 4, width, height, x_min, x_max, y_min, y_max,
                                   MAX_ITERATIONS);
        double end = get_time_sec();

        double elapsed = end - start;
        total_time += elapsed;
        printf("  Frame %2d: %.4f seconds (%.2f fps)\n", i + 1, elapsed, 1.0 / elapsed);
    }

    double avg_time = total_time / NUM_RUNS;
    double avg_fps = 1.0 / avg_time;
    double mpx_s = (total_pixels / 1e6) / avg_time;

    printf("\nResults:\n");
    printf("  -> Average time: %.4f seconds\n", avg_time);
    printf("  -> Average FPS:  %.2f fps\n", avg_fps);
    printf("  -> Speed:        %.2f Mpx/s\n\n", mpx_s);

    free(pixels);
}

int main() {
    printf("Initializing renderer...\n");
    init_renderer(MAX_ITERATIONS, 0);  // 0 is default palette

    // 1080p benchmark
    run_benchmark("1. Renderer 1080p Benchmark (64-bit)", 1920, 1080);

    // 4k benchmark
    run_benchmark("2. Renderer 4K Benchmark (64-bit)", 3840, 2160);

#ifdef USE_SIMD_F128
    // 1080p benchmark 128-bit
    set_cpu_precision(1);
    run_benchmark("3. Renderer 1080p Benchmark (128-bit)", 1920, 1080);

    // 4k benchmark 128-bit
    run_benchmark("4. Renderer 4K Benchmark (128-bit)", 3840, 2160);
    set_cpu_precision(0);
#endif

    cleanup_renderer();
    printf("Benchmark complete.\n");
    return 0;
}
