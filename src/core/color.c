#include "color.h"

#include <math.h>

#include "config.h"

static uint8_t color_lut[MAX_ITERATIONS_LIMIT + 1][3];

const char* PALETTE_NAMES[PALETTE_COUNT] = {"Sine Wave", "Grayscale", "Fire",
                                            "Electric",  "Ocean",     "Inferno"};

void init_color_palette(int max_iterations, int palette_idx) {
    for (int i = 0; i < max_iterations; i++) {
        switch (palette_idx) {
            case 0:
                color_lut[i][0] = (uint8_t)(sin(0.1 * i + 0) * 127 + 128);
                color_lut[i][1] = (uint8_t)(sin(0.1 * i + 2) * 127 + 128);
                color_lut[i][2] = (uint8_t)(sin(0.1 * i + 4) * 127 + 128);
                break;
            case 1:
                color_lut[i][0] = color_lut[i][1] = color_lut[i][2] = (uint8_t)(i % 256);
                break;
            case 2:
                color_lut[i][0] = (uint8_t)fmin(255, i * 4);
                color_lut[i][1] = (uint8_t)fmin(255, i * 2);
                color_lut[i][2] = (uint8_t)fmin(255, i * 1);
                break;
            case 3:
                color_lut[i][0] = (uint8_t)fmin(255, i * 1);
                color_lut[i][1] = (uint8_t)fmin(255, i * 4);
                color_lut[i][2] = (uint8_t)fmin(255, i * 8);
                break;
            case 4:
                color_lut[i][0] = (uint8_t)fmin(255, i * 0.5);
                color_lut[i][1] = (uint8_t)fmin(255, i * 2);
                color_lut[i][2] = (uint8_t)fmin(255, i * 5);
                break;
            case 5:
                color_lut[i][0] = (uint8_t)fmin(255, i * 8);
                color_lut[i][1] = (uint8_t)fmin(255, i * 2);
                color_lut[i][2] = (uint8_t)fmin(255, i * 0.5);
                break;
            default:
                color_lut[i][0] = color_lut[i][1] = color_lut[i][2] = 127;
                break;
        }
    }
    color_lut[max_iterations][0] = 0;
    color_lut[max_iterations][1] = 0;
    color_lut[max_iterations][2] = 0;
}

void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (iterations >= max_iterations) {
        *r = *g = *b = 0;
        return;
    }

    if (iterations < 0) iterations = 0;
    if (iterations > max_iterations) iterations = (double)max_iterations;

    int i = (int)iterations;
    double t = iterations - i;

    int i2 = i + 1;
    if (i2 > max_iterations) i2 = max_iterations;

    *r = (uint8_t)(color_lut[i][0] * (1.0 - t) + color_lut[i2][0] * t);
    *g = (uint8_t)(color_lut[i][1] * (1.0 - t) + color_lut[i2][1] * t);
    *b = (uint8_t)(color_lut[i][2] * (1.0 - t) + color_lut[i2][2] * t);
}