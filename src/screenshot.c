#include "screenshot.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Minimal PNG encoder -- no external libraries beyond zlib.
 *
 * PNG structure:
 *   8-byte signature
 *   IHDR chunk  (image metadata)
 *   IDAT chunk  (zlib-compressed, filter-byte-prefixed pixel rows)
 *   IEND chunk  (empty, marks end of file)
 *
 * Each chunk is:  [4-byte length] [4-byte type] [data] [4-byte CRC32]
 * CRC32 covers the type + data bytes (not the length field).
 * -------------------------------------------------------------------------
 */

/* Write a 32-bit value in big-endian byte order. */
static void write_u32_be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v      );
}

/* Write a PNG chunk to the file. CRC32 is computed automatically. */
static void write_chunk(FILE *f, const char type[4],
                         const uint8_t *data, uint32_t len) {
    /* Length field (does not include type or CRC) */
    uint8_t hdr[4];
    write_u32_be(hdr, len);
    fwrite(hdr, 1, 4, f);

    /* Type */
    fwrite(type, 1, 4, f);

    /* Data */
    if (len > 0)
        fwrite(data, 1, len, f);

    /* CRC32 covers type + data */
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if (len > 0)
        crc = crc32(crc, data, len);

    uint8_t crc_buf[4];
    write_u32_be(crc_buf, (uint32_t)crc);
    fwrite(crc_buf, 1, 4, f);
}

/*
 * Encode RGBA pixels as a PNG file.
 * Returns 1 on success, 0 on any error.
 */
static int write_png(const char *path, int width, int height,
                      const uint8_t *rgba) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    /* PNG magic bytes */
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);

    /* IHDR -- 13 bytes of image metadata */
    uint8_t ihdr[13] = {0};
    write_u32_be(ihdr + 0, (uint32_t)width);
    write_u32_be(ihdr + 4, (uint32_t)height);
    ihdr[8]  = 8; /* bit depth: 8 bits per channel */
    ihdr[9]  = 6; /* colour type: RGBA (truecolour + alpha) */
    /* ihdr[10..12]: compression, filter, interlace all default to 0 */
    write_chunk(f, "IHDR", ihdr, 13);

    /* Build the raw image stream: each row is prefixed by a filter byte.
     * Filter type 0 (None) means the row is stored as-is -- simple and fast. */
    int row_stride      = width * 4;            /* bytes per row (RGBA) */
    size_t raw_size     = (size_t)(row_stride + 1) * (size_t)height;
    uint8_t *raw        = malloc(raw_size);
    if (!raw) { fclose(f); return 0; }

    for (int y = 0; y < height; y++) {
        raw[y * (row_stride + 1)] = 0;  /* filter byte: None */
        memcpy(raw + y * (row_stride + 1) + 1,
               rgba + y * row_stride,
               (size_t)row_stride);
    }

    /* Compress the raw stream with zlib */
    uLongf compressed_bound = compressBound((uLong)raw_size);
    uint8_t *compressed     = malloc(compressed_bound);
    if (!compressed) { free(raw); fclose(f); return 0; }

    uLongf compressed_size = compressed_bound;
    int ret = compress2(compressed, &compressed_size,
                        raw, (uLong)raw_size, Z_BEST_SPEED);
    free(raw);

    if (ret != Z_OK) { free(compressed); fclose(f); return 0; }

    /* IDAT -- the compressed pixel data */
    write_chunk(f, "IDAT", compressed, (uint32_t)compressed_size);
    free(compressed);

    /* IEND -- empty end-of-file marker */
    write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    return 1;
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

void save_screenshot(SDL_Renderer *renderer, int width, int height) {
    /* Read the current frame from the renderer into an RGBA pixel buffer */
    uint8_t *pixels = malloc((size_t)width * (size_t)height * 4);
    if (!pixels) {
        fprintf(stderr, "Screenshot: out of memory\n");
        return;
    }

    /* SDL_PIXELFORMAT_RGBA32 is RGBA on all platforms */
    if (SDL_RenderReadPixels(renderer, NULL,
                              SDL_PIXELFORMAT_RGBA32,
                              pixels, width * 4) != 0) {
        fprintf(stderr, "Screenshot: SDL_RenderReadPixels failed: %s\n",
                SDL_GetError());
        free(pixels);
        return;
    }

    /* Build a timestamped filename so each screenshot has a unique name */
    char filename[64];
    time_t     now = time(NULL);
    struct tm *tm  = localtime(&now);
    strftime(filename, sizeof(filename), "mandelbrot_%Y%m%d_%H%M%S.png", tm);

    if (write_png(filename, width, height, pixels))
        printf("Screenshot saved: %s\n", filename);
    else
        fprintf(stderr, "Screenshot: failed to write %s\n", filename);

    free(pixels);
}
