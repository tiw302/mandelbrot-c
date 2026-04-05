#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>
#include "screenshot.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t  id_length;
    uint8_t  color_map_type;
    uint8_t  image_type;
    uint16_t color_map_origin;
    uint16_t color_map_length;
    uint8_t  color_map_depth;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t  pixel_depth;
    uint8_t  image_descriptor;
} TgaHeader;
#pragma pack(pop)

static void save_tga(const char *filename, uint32_t *pixels, int w, int h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    TgaHeader header = {0};
    header.image_type = 2; // raw true color
    header.width = w;
    header.height = h;
    header.pixel_depth = 32;
    header.image_descriptor = 0x20; // start from top left

    fwrite(&header, sizeof(header), 1, f);
    // requires BGRA format
    uint32_t *buf = malloc(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        uint8_t a = (pixels[i] >> 24) & 0xFF;
        uint8_t r = (pixels[i] >> 16) & 0xFF;
        uint8_t g = (pixels[i] >> 8) & 0xFF;
        uint8_t b = pixels[i] & 0xFF;
        buf[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    fwrite(buf, w * h * 4, 1, f);
    free(buf);
    fclose(f);
}

int save_screenshot(SDL_Renderer *renderer, int width, int height) {
    uint32_t *pixels = malloc(width * height * 4);
    if (!pixels) return -1;

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels, width * 4) != 0) {
        free(pixels);
        return -1;
    }

    char filename[64];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.tga", tm);

    save_tga(filename, pixels, width, height);
    printf("Saved screenshot to %s\n", filename);

    free(pixels);
    return 0;
}
