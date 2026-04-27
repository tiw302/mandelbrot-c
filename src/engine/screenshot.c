#include "screenshot.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "stb/stb_image_write.h"

void save_screenshot(uint32_t* pixels, int width, int height) {
    char filename[64];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(filename, sizeof(filename), "mandelbrot_%Y%m%d_%H%M%S.png", t);

    /* Allocate temp buffer for RGBA conversion */
    uint32_t* rgba_pixels = (uint32_t*)malloc(width * height * 4);
    if (!rgba_pixels) return;

    for (int i = 0; i < width * height; i++) {
        uint32_t p = pixels[i];
        /* Swap Red (bit 16-23) and Blue (bit 0-7) */
        uint8_t b = (p >> 0) & 0xFF;
        uint8_t g = (p >> 8) & 0xFF;
        uint8_t r = (p >> 16) & 0xFF;
        uint8_t a = (p >> 24) & 0xFF;
        rgba_pixels[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    }

    if (stbi_write_png(filename, width, height, 4, rgba_pixels, width * 4)) {
        printf("screenshot saved to %s\n", filename);
    } else {
        fprintf(stderr, "error: failed to save screenshot\n");
    }

    free(rgba_pixels);
}