#include "color.h"

#include <math.h>
#include <stdlib.h>

// array of available color palette names for the ui.
const char* PALETTE_NAMES[PALETTE_COUNT] = {"Sine Wave", "Grayscale", "Fire",   "Electric", "Ocean",
                                            "Inferno",   "Viridis",   "Plasma", "Twilight"};

// structure representing an rgb color.
typedef struct {
    uint8_t r, g, b;
} rgb_t;

/* global state for the color palette
 * including the current palette index,
 * lookup table pointer, and allocated size. */
static int current_palette = 0;
static rgb_t* palette_lut = NULL;
static int lut_size = 0;

/* initializes the color palette lookup table.
 * reallocates the table if the needed size increases
 * and precomputes rgb values based on the selected palette.
 * returns 1 on success, 0 on memory allocation failure. */
int init_color_palette(int max_iterations, int palette_idx) {
    current_palette = palette_idx;

    int needed_size = max_iterations + 2;
    if (!palette_lut || lut_size < needed_size) {
        rgb_t* new_lut = (rgb_t*)realloc(palette_lut, sizeof(rgb_t) * needed_size);
        if (new_lut) {
            palette_lut = new_lut;
            lut_size = needed_size;
        } else {
            return 0;
        }
    }

    if (palette_lut) {
        for (int fi = 0; fi < needed_size; fi++) {
            double r_val, g_val, b_val;
            double dfi = (double)fi;

            switch (current_palette) {
                case 0:  // sine wave
                    r_val = sin(0.1 * dfi + 4.0) * 127.0 + 128.0;
                    g_val = sin(0.1 * dfi + 2.0) * 127.0 + 128.0;
                    b_val = sin(0.1 * dfi + 0.0) * 127.0 + 128.0;
                    break;
                case 1:  // grayscale
                    r_val = g_val = b_val = fmod(dfi, 256.0);
                    break;
                case 2:  // fire
                    r_val = 255.0 - fabs(fmod(dfi * 1.0, 510.0) - 255.0);
                    g_val = 255.0 - fabs(fmod(dfi * 2.0, 510.0) - 255.0);
                    b_val = 255.0 - fabs(fmod(dfi * 4.0, 510.0) - 255.0);
                    break;
                case 3:  // electric
                    r_val = 255.0 - fabs(fmod(dfi * 8.0, 510.0) - 255.0);
                    g_val = 255.0 - fabs(fmod(dfi * 4.0, 510.0) - 255.0);
                    b_val = 255.0 - fabs(fmod(dfi * 1.0, 510.0) - 255.0);
                    break;
                case 4:  // ocean
                    r_val = 255.0 - fabs(fmod(dfi * 5.0, 510.0) - 255.0);
                    g_val = 255.0 - fabs(fmod(dfi * 2.0, 510.0) - 255.0);
                    b_val = 255.0 - fabs(fmod(dfi * 0.5, 510.0) - 255.0);
                    break;
                case 5:  // inferno
                    r_val = 255.0 - fabs(fmod(dfi * 0.5, 510.0) - 255.0);
                    g_val = 255.0 - fabs(fmod(dfi * 2.0, 510.0) - 255.0);
                    b_val = 255.0 - fabs(fmod(dfi * 8.0, 510.0) - 255.0);
                    break;
                case 6: {  // viridis
                    // use ping-pong wrap to prevent sharp edges when color cycles
                    double t = 1.0 - fabs(fmod(dfi / 256.0, 2.0) - 1.0);
                    r_val = 255.0 * (0.267 + t * (0.993 * t - 0.260));
                    g_val = 255.0 * (0.004 + t * (1.490 - t * 0.494));
                    b_val = 255.0 * (0.329 + t * (1.268 * t * t - 0.680 * t - 0.259));
                    break;
                }
                case 7: {  // plasma
                    // ping-pong wrap here as well
                    double t = 1.0 - fabs(fmod(dfi / 256.0, 2.0) - 1.0);
                    r_val = 255.0 * (0.050 + t * (2.735 - t * 1.785));
                    g_val = 255.0 * fmax(0.0, t * (1.580 * t - 0.580));
                    b_val = 255.0 * fmax(0.0, 0.530 + t * (0.750 - t * 1.280));
                    break;
                }
                case 8: {  // twilight
                    double t = fmod(dfi / 128.0, 1.0);
                    r_val = 255.0 * (0.5 + 0.5 * sin(6.283 * t + 0.0));
                    g_val = 255.0 * (0.3 + 0.2 * sin(6.283 * t + 2.094));
                    b_val = 255.0 * (0.5 + 0.5 * sin(6.283 * t + 4.189));
                    break;
                }
                default:
                    r_val = g_val = b_val = 0;
                    break;
            }

            palette_lut[fi].r = (uint8_t)fmin(255.0, fmax(0.0, r_val));
            palette_lut[fi].g = (uint8_t)fmin(255.0, fmax(0.0, g_val));
            palette_lut[fi].b = (uint8_t)fmin(255.0, fmax(0.0, b_val));
        }
    }

    return 1;
}

/* retrieves the interpolated color for a given iteration count.
 * returns black if iterations exceed max, otherwise interpolates
 * between adjacent colors in the lookup table. */
void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (iterations >= max_iterations) {
        *r = *g = *b = 0;
        return;
    }

    if (!palette_lut) {
        *r = *g = *b = 0;
        return;
    }

    int fi = (int)iterations;
    double frac = iterations - (double)fi;

    if (fi < 0) fi = 0;

    if (lut_size < 2) {
        *r = *g = *b = 0;
        return;
    }

    if (fi >= lut_size - 1) fi = lut_size - 2;

    rgb_t c1 = palette_lut[fi];
    rgb_t c2 = palette_lut[fi + 1];

    *r = (uint8_t)(c1.r + frac * (c2.r - c1.r));
    *g = (uint8_t)(c1.g + frac * (c2.g - c1.g));
    *b = (uint8_t)(c1.b + frac * (c2.b - c1.b));
}

// frees the allocated memory for the palette
// lookup table and resets the global state.
void cleanup_color_palette(void) {
    if (palette_lut) {
        free(palette_lut);
        palette_lut = NULL;
    }
    lut_size = 0;
}