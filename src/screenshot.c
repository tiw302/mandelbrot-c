#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>
#include "screenshot.h"

static void write_u32_be(FILE *f, uint32_t v) {
    uint8_t buf[4] = {(v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF};
    fwrite(buf, 4, 1, f);
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    write_u32_be(f, len);
    fwrite(type, 4, 1, f);
    if (len > 0) fwrite(data, len, 1, f);
    
    uint32_t crc = (uint32_t)crc32(0, (const uint8_t *)type, 4);
    if (len > 0) crc = (uint32_t)crc32(crc, data, (uInt)len);
    write_u32_be(f, crc);
}

static void save_png(const char *filename, uint32_t *pixels, int w, int h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    fwrite(sig, 8, 1, f);

    // IHDR: 13 bytes
    uint8_t ihdr_data[13];
    ihdr_data[0] = (w >> 24) & 0xFF; ihdr_data[1] = (w >> 16) & 0xFF; ihdr_data[2] = (w >> 8) & 0xFF; ihdr_data[3] = w & 0xFF;
    ihdr_data[4] = (h >> 24) & 0xFF; ihdr_data[5] = (h >> 16) & 0xFF; ihdr_data[6] = (h >> 8) & 0xFF; ihdr_data[7] = h & 0xFF;
    ihdr_data[8] = 8; // Bit depth
    ihdr_data[9] = 6; // Color type: Truecolor with alpha (RGBA)
    ihdr_data[10] = 0; // Compression: Deflate
    ihdr_data[11] = 0; // Filter: Adaptive
    ihdr_data[12] = 0; // Interlace: No
    write_chunk(f, "IHDR", ihdr_data, 13);

    // Prepare raw data for deflate: filter 0 + row data
    size_t row_size = (size_t)w * 4 + 1;
    size_t raw_size = (size_t)h * row_size;
    uint8_t *raw = malloc(raw_size);
    if (!raw) {
        fclose(f);
        return;
    }

    for (int y = 0; y < h; y++) {
        raw[y * row_size] = 0; // Filter type 0
        for (int x = 0; x < w; x++) {
            uint32_t p = pixels[y * w + x];
            uint8_t *dest = &raw[y * row_size + 1 + x * 4];
            // SDL_PIXELFORMAT_ARGB8888 -> R G B A
            dest[0] = (p >> 16) & 0xFF; // R
            dest[1] = (p >> 8)  & 0xFF; // G
            dest[2] = p         & 0xFF; // B
            dest[3] = (p >> 24) & 0xFF; // A
        }
    }

    uLongf comp_size = compressBound((uLong)raw_size);
    uint8_t *comp = malloc(comp_size);
    if (comp) {
        if (compress(comp, &comp_size, raw, (uLong)raw_size) == Z_OK) {
            write_chunk(f, "IDAT", comp, (uint32_t)comp_size);
        }
        free(comp);
    }

    write_chunk(f, "IEND", NULL, 0);

    free(raw);
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
    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.png", tm);

    save_png(filename, pixels, width, height);
    printf("Saved screenshot to %s\n", filename);

    free(pixels);
    return 0;
}
