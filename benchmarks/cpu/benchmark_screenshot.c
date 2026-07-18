/* benchmark_screenshot.c
 *
 * performance benchmarking for screenshot image encoding and disk i/o.
 * tests standard png saving vs chunked mega screenshot tga saving.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

#include "renderer.h"
#include "screenshot.h"

#define MAX_ITERATIONS 1000

// returns time in seconds
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// helper to delete files generated during benchmark
static void remove_latest_file(const char* prefix, const char* ext) {
    /* simplified cleanup: just inform user.     * robust file globbing in pure c is platform
     * dependent. */
    printf("  (note: please clean up %s*.%s manually if needed)\n", prefix, ext);
}

int main(void) {
    printf("==========================================\n");
    printf(" Screenshot I/O Benchmarks                \n");
    printf("==========================================\n\n");

    RendererContext* ctx = init_renderer(MAX_ITERATIONS, 0);
    if (!ctx) {
        printf("failed to initialize renderer\n");
        return 1;
    }

    const double x_min = -2.0;
    const double x_max = 1.0;
    const double y_min = -1.5;
    const double y_max = 1.5;
    complex_t dummy = {0};
    AppCommonState dummy_state = {0};

    // standard png screenshot (1080p)
    int width_1080 = 1920;
    int height_1080 = 1080;
    printf("1. Standard PNG Screenshot (%dx%d)\n", width_1080, height_1080);

    uint32_t* pixels = (uint32_t*)malloc((size_t)width_1080 * height_1080 * sizeof(uint32_t));
    if (pixels) {
        // pre-render an image so we have realistic data to compress
        printf("   (pre-rendering 1080p buffer...)\n");
        complex_t julia_c = {0.0, 0.0};
        RenderJob job = {.pixels = pixels,
                         .pitch = width_1080 * 4,
                         .window_width = width_1080,
                         .window_height = height_1080,
                         .re_min = x_min,
                         .re_max = x_max,
                         .im_top = y_min,
                         .im_bottom = y_max,
                         .mode = RENDER_MANDELBROT,
                         .julia_c = julia_c,
                         .max_iterations = MAX_ITERATIONS};
        render_fractal_threaded(ctx, &job);

        printf("   Saving PNG to disk...\n");
        double start = get_time_sec();
        save_screenshot(NULL, pixels, width_1080, height_1080, 0, 0, 0);
        double end = get_time_sec();

        printf("   -> Time taken: %.4f seconds\n\n", end - start);
        remove_latest_file("mandelbrot_", "png");
        free(pixels);
    }

    /* mega screenshot (8k tga)     * 8K UHD is 7680x4320 */
    int width_8k = 7680;
    int height_8k = 4320;
    printf("2. Mega Screenshot TGA (%dx%d - %.2f Mpx)\n", width_8k, height_8k,
           (width_8k * height_8k) / 1e6);
    printf("   Rendering and saving chunks to disk...\n");

    double start = get_time_sec();
    save_mega_screenshot(ctx, &dummy_state, width_8k, height_8k, x_min, x_max, y_min, y_max,
                         MAX_ITERATIONS, 0, dummy);
    double end = get_time_sec();

    printf("   -> Time taken: %.4f seconds\n\n", end - start);
    remove_latest_file("mega_mandelbrot_", "tga");

    cleanup_renderer(ctx);
    printf("Benchmark complete.\n");
    return 0;
}
