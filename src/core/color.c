#include "color.h"

#include <math.h>
#include <stdlib.h>

const char* PALETTE_NAMES[PALETTE_COUNT] = {"Sine Wave", "Grayscale", "Fire",
                                            "Electric",  "Ocean",     "Inferno",
                                            "Viridis",   "Plasma",    "Twilight"};

static int current_palette = 0;

/* init_color_palette — set the active palette.
 * max_iterations is accepted for forward compatibility (e.g. precomputed
 * lookup tables) but currently unused. */
void init_color_palette(int max_iterations, int palette_idx) {
    (void)max_iterations;
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
        case 6: { /* viridis — perceptually uniform, colorblind-safe */
            double t1 = fmod(fi / 256.0, 1.0);
            double t2 = fmod((fi + 1.0) / 256.0, 1.0);
            r1 = 255.0 * (0.267 + t1 * (0.993 * t1 - 0.260));
            g1 = 255.0 * (0.004 + t1 * (1.490 - t1 * 0.494));
            b1 = 255.0 * (0.329 + t1 * (1.268 * t1 * t1 - 0.680 * t1 - 0.259));
            r2 = 255.0 * (0.267 + t2 * (0.993 * t2 - 0.260));
            g2 = 255.0 * (0.004 + t2 * (1.490 - t2 * 0.494));
            b2 = 255.0 * (0.329 + t2 * (1.268 * t2 * t2 - 0.680 * t2 - 0.259));
            break;
        }
        case 7: { /* plasma — warm purple-to-yellow gradient */
            double t1 = fmod(fi / 256.0, 1.0);
            double t2 = fmod((fi + 1.0) / 256.0, 1.0);
            r1 = 255.0 * (0.050 + t1 * (2.735 - t1 * 1.785));
            g1 = 255.0 * fmax(0.0, t1 * (1.580 * t1 - 0.580));
            b1 = 255.0 * fmax(0.0, 0.530 + t1 * (0.750 - t1 * 1.280));
            r2 = 255.0 * (0.050 + t2 * (2.735 - t2 * 1.785));
            g2 = 255.0 * fmax(0.0, t2 * (1.580 * t2 - 0.580));
            b2 = 255.0 * fmax(0.0, 0.530 + t2 * (0.750 - t2 * 1.280));
            break;
        }
        case 8: { /* twilight — cyclic purple-orange-purple, good for periodic data */
            double t1 = fmod(fi / 128.0, 1.0);
            double t2 = fmod((fi + 1.0) / 128.0, 1.0);
            r1 = 255.0 * (0.5 + 0.5 * sin(6.283 * t1 + 0.0));
            g1 = 255.0 * (0.3 + 0.2 * sin(6.283 * t1 + 2.094));
            b1 = 255.0 * (0.5 + 0.5 * sin(6.283 * t1 + 4.189));
            r2 = 255.0 * (0.5 + 0.5 * sin(6.283 * t2 + 0.0));
            g2 = 255.0 * (0.3 + 0.2 * sin(6.283 * t2 + 2.094));
            b2 = 255.0 * (0.5 + 0.5 * sin(6.283 * t2 + 4.189));
            break;
        }
        default:
            r1 = g1 = b1 = r2 = g2 = b2 = 0;
            break;
    }

    /* clamp to [0, 255] before casting — polynomial palettes (viridis, plasma)
     * can produce slightly out-of-range values at boundary t values,
     * and casting a negative or >255 double to uint8_t is undefined behavior. */
    *r = (uint8_t)fmin(255.0, fmax(0.0, r1 + frac * (r2 - r1)));
    *g = (uint8_t)fmin(255.0, fmax(0.0, g1 + frac * (g2 - g1)));
    *b = (uint8_t)fmin(255.0, fmax(0.0, b1 + frac * (b2 - b1)));
}