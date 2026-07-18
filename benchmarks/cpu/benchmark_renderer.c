/* benchmark_renderer.c
 *
 * performance benchmarking for the multi-threaded cpu renderer.
 * tests thread pool load balancing and pixel memory write throughput.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "renderer.h"

#define MAX_ITERATIONS 1000
#define NUM_RUNS 10

// returns time in seconds
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void run_benchmark(RendererContext* ctx, const char* name, int width, int height) {
    printf("==========================================\n");
    printf(" %s\n", name);
    printf(" Resolution: %d x %d (%.2f Million Pixels)\n", width, height, (width * height) / 1e6);
    printf(" Max Iterations: %d\n", MAX_ITERATIONS);
    printf(" Thread Count: %d\n", get_actual_thread_count(ctx));
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

    complex_t julia_c = {0.0, 0.0};
    RenderJob job = {.pixels = pixels,
                     .pitch = width * 4,
                     .window_width = width,
                     .window_height = height,
                     .re_min = x_min,
                     .re_max = x_max,
                     .im_top = y_min,
                     .im_bottom = y_max,
                     .mode = RENDER_MANDELBROT,
                     .julia_c = julia_c,
                     .max_iterations = MAX_ITERATIONS};
    // warm up run
    render_fractal_threaded(ctx, &job);

    for (int i = 0; i < NUM_RUNS; i++) {
        double start = get_time_sec();
        render_fractal_threaded(ctx, &job);
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

static void run_scaling_sweep(RendererContext* ctx, int width, int height) {
    printf("==========================================\n");
    printf(" Thread Scaling Sweep (%d x %d)\n", width, height);
    printf("==========================================\n");

    uint32_t* pixels = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    if (!pixels) {
        printf("error: memory allocation failed for scaling sweep\n");
        return;
    }

    const double x_min = -2.0;
    const double x_max = 1.0;
    const double y_min = -1.5;
    const double y_max = 1.5;

    int threads_to_test[16];
    int num_tests = 0;
    threads_to_test[num_tests++] = 1;
    int optimal = get_optimal_thread_count();
    for (int t = 2; t <= optimal; t *= 2) {
        threads_to_test[num_tests++] = t;
    }
    if (threads_to_test[num_tests - 1] != optimal) {
        threads_to_test[num_tests++] = optimal;
    }

    double time_1 = 1.0;  // placeholder for speedup calculations

    printf("%-10s | %-12s | %-10s | %-12s | %-10s\n", "threads", "avg time", "speedup",
           "efficiency", "mpx/s");
    printf("-----------------------------------------------------------------\n");

    for (int idx = 0; idx < num_tests; idx++) {
        int t = threads_to_test[idx];
        set_renderer_thread_count(ctx, t);

        complex_t julia_c = {0.0, 0.0};
        RenderJob job = {.pixels = pixels,
                         .pitch = width * 4,
                         .window_width = width,
                         .window_height = height,
                         .re_min = x_min,
                         .re_max = x_max,
                         .im_top = y_min,
                         .im_bottom = y_max,
                         .mode = RENDER_MANDELBROT,
                         .julia_c = julia_c,
                         .max_iterations = MAX_ITERATIONS};
        // warm up run
        render_fractal_threaded(ctx, &job);

        // measure runs
        double total_time = 0.0;
        int runs = (t == 1) ? 2 : 3;  // run less for 1 thread to speed it up
        for (int r = 0; r < runs; r++) {
            double start = get_time_sec();
            render_fractal_threaded(ctx, &job);
            double end = get_time_sec();
            total_time += (end - start);
        }

        double avg_time = total_time / runs;
        if (t == 1) {
            time_1 = avg_time;
        }

        double speedup = time_1 / avg_time;
        double efficiency = (speedup / t) * 100.0;
        double mpx_s = ((double)width * height / 1e6) / avg_time;

        printf("%-10d | %-10.4fs | %-8.2fx | %-10.1f%% | %-10.2f\n", t, avg_time, speedup,
               efficiency, mpx_s);
    }
    printf("-----------------------------------------------------------------\n\n");

    free(pixels);
}

int main(void) {
    printf("Initializing renderer...\n");
    RendererContext* ctx = init_renderer(MAX_ITERATIONS, 0);  // 0 is default palette
    if (!ctx) {
        printf("failed to initialize renderer\n");
        return 1;
    }

    // run the thread scaling sweep first
    run_scaling_sweep(ctx, 1920, 1080);

    // restore optimal threads for the rest of the benchmarks
    set_renderer_thread_count(ctx, get_optimal_thread_count());

    // 1080p benchmark
    run_benchmark(ctx, "1. Renderer 1080p Benchmark (64-bit)", 1920, 1080);

    // 4k benchmark
    run_benchmark(ctx, "2. Renderer 4K Benchmark (64-bit)", 3840, 2160);

#ifdef USE_SIMD_F128
    // 1080p benchmark 128-bit
    set_cpu_precision(ctx, 1);
    run_benchmark(ctx, "3. Renderer 1080p Benchmark (128-bit)", 1920, 1080);

    // 4k benchmark 128-bit
    run_benchmark(ctx, "4. Renderer 4K Benchmark (128-bit)", 3840, 2160);
    set_cpu_precision(ctx, 0);
#endif

    cleanup_renderer(ctx);
    printf("Benchmark complete.\n");
    return 0;
}
