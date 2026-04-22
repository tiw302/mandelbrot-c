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

    if (stbi_write_png(filename, width, height, 4, pixels, width * 4)) {
        printf("screenshot saved to %s\n", filename);
    } else {
        fprintf(stderr, "error: failed to save screenshot\n");
    }
}