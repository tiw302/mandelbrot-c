#include "color.h"

#include <math.h>
#include <stdlib.h>

const char* PALETTE_NAMES[PALETTE_COUNT] = {"Sine Wave", "Grayscale", "Fire",
                                            "Electric",  "Ocean",     "Inferno"};

static int current_palette = 0;

void init_color_palette(int max_iterations, int palette_idx) {
    current_palette = palette_idx;
}

void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (iterations >= max_iterations) {
        *r = *g = *b = 0;
        return;
    }

    double i = iterations;
    double fi = floor(i);
    double frac = i - fi;

    double r1, g1, b1, r2, g2, b2;

    switch (current_palette) {
        case 0: /* sine wave: use 4, 2, 0 for mint */
            r1 = sin(0.1 * fi + 4.0) * 127.0 + 128.0;
            g1 = sin(0.1 * fi + 2.0) * 127.0 + 128.0;
            b1 = sin(0.1 * fi + 0.0) * 127.0 + 128.0;
            r2 = sin(0.1 * (fi + 1.0) + 4.0) * 127.0 + 128.0;
            g2 = sin(0.1 * (fi + 1.0) + 2.0) * 127.0 + 128.0;
            b2 = sin(0.1 * (fi + 1.0) + 0.0) * 127.0 + 128.0;
            break;
        case 1: /* grayscale */
            r1 = g1 = b1 = fmod(fi, 256.0);
            r2 = g2 = b2 = fmod(fi + 1.0, 256.0);
            break;
        case 2: /* fire */
            r1 = fmin(255.0, fi * 1.0);
            g1 = fmin(255.0, fi * 2.0);
            b1 = fmin(255.0, fi * 4.0);
            r2 = fmin(255.0, (fi + 1.0) * 1.0);
            g2 = fmin(255.0, (fi + 1.0) * 2.0);
            b2 = fmin(255.0, (fi + 1.0) * 4.0);
            break;
        case 3: /* electric */
            r1 = fmin(255.0, fi * 8.0);
            g1 = fmin(255.0, fi * 4.0);
            b1 = fmin(255.0, fi * 1.0);
            r2 = fmin(255.0, (fi + 1.0) * 8.0);
            g2 = fmin(255.0, (fi + 1.0) * 4.0);
            b2 = fmin(255.0, (fi + 1.0) * 1.0);
            break;
        case 4: /* ocean */
            r1 = fmin(255.0, fi * 5.0);
            g1 = fmin(255.0, fi * 2.0);
            b1 = fmin(255.0, fi * 0.5);
            r2 = fmin(255.0, (fi + 1.0) * 5.0);
            g2 = fmin(255.0, (fi + 1.0) * 2.0);
            b2 = fmin(255.0, (fi + 1.0) * 0.5);
            break;
        case 5: /* inferno */
            r1 = fmin(255.0, fi * 0.5);
            g1 = fmin(255.0, fi * 2.0);
            b1 = fmin(255.0, fi * 8.0);
            r2 = fmin(255.0, (fi + 1.0) * 0.5);
            g2 = fmin(255.0, (fi + 1.0) * 2.0);
            b2 = fmin(255.0, (fi + 1.0) * 8.0);
            break;
        default:
            r1 = g1 = b1 = r2 = g2 = b2 = 0;
            break;
    }

    *r = (uint8_t)(r1 + frac * (r2 - r1));
    *g = (uint8_t)(g1 + frac * (g2 - g1));
    *b = (uint8_t)(b1 + frac * (b2 - b1));
}