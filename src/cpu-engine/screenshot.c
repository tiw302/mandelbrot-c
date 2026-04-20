#include "screenshot.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void save_screenshot(uint32_t *pixels, int width, int height) {
    char filename[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(filename, sizeof(filename), "mandelbrot_%Y%m%d_%H%M%S.png", t);

    // stb_image_write expects pixels in a certain format. 
    // Our pixels are 0xAABBGGRR or 0xAARRGGBB depending on the engine.
    // Most professional way is to use it directly if the format matches.
    // STB_image_write is very robust.
    
    if (stbi_write_png(filename, width, height, 4, pixels, width * 4)) {
        printf("Screenshot saved to %s\n", filename);
    } else {
        fprintf(stderr, "Error: Failed to save screenshot\n");
    }
}
