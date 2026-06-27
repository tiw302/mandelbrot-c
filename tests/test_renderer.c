// test_renderer.c — unit test for the multithreaded renderer dispatch pool.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "renderer.h"

int test_pool_dispatch() {
    int max_iterations = 100;
    int palette_idx = 0;

    // initialize renderer with dummy max iterations and palette
    RendererContext* ctx = init_renderer(max_iterations, palette_idx);
    if (!ctx) {
        printf("failed: could not initialize renderer context\n");
        return 0;
    }

    // create a dummy pixel buffer
    int width = 128;
    int height = 128;
    int pitch = width * sizeof(uint32_t);
    uint32_t* pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    if (!pixels) {
        printf("failed: could not allocate dummy pixel buffer\n");
        cleanup_renderer(ctx);
        return 0;
    }

    // dispatch a small render job
    render_mandelbrot_threaded(ctx, pixels, pitch, width, height, -2.0, 1.0, 1.5, -1.5, max_iterations);

    // validate some pixels got modified (at least the center shouldn't be all 0 if mapped properly)
    int modified = 0;
    for (int i = 0; i < width * height; i++) {
        if (pixels[i] != 0) {
            modified = 1;
            break;
        }
    }

    if (!modified) {
        printf("failed: pixels were not modified by dispatch\n");
        free(pixels);
        cleanup_renderer(ctx);
        return 0;
    }

    free(pixels);
    cleanup_renderer(ctx);
    return 1;
}

int main() {
    int passed = 0;
    int failed = 0;

    printf("running renderer thread pool tests...\n");

    if (test_pool_dispatch()) {
        printf("  [pass] test_pool_dispatch\n");
        passed++;
    } else {
        printf("  [FAIL] test_pool_dispatch\n");
        failed++;
    }

    printf("\nresults: %d passed, %d failed\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
