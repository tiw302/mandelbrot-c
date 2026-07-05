/* color.c
 *
 * color palette initialization, interpolation, and lookup functions.
 * generates smooth transitions between colors to prevent color banding.
 */

#include "color.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int current_palette = 0;
static uint32_t* palette_lut_argb = NULL;
static int lut_size = 0;
static int current_max_iterations = 1000;

#define MAX_OLD_LUTS 32
static uint32_t* old_luts[MAX_OLD_LUTS];
static int num_old_luts = 0;

#define NUM_BUILTIN_PALETTES 22
static const char* builtin_palette_names[NUM_BUILTIN_PALETTES] = {
    "Sine Wave", "Volumetric Magma", "Viridis",
    "Grayscale", "Electric", "Ocean", "Inferno", "Retro Binary",
    "Orbit Mesh (GPU)", "Biomorph Trap (GPU)", "Conformal Ripples (GPU)", "Curvature Marble (GPU)",
    "Conformal Grid (GPU)", "Cyber Grid (GPU)",
    "Bubble Pearl (GPU)", "Liquid Chrome (GPU)", "Refractive 3D Glass (GPU)",
    "Ultra Fractal Classic (GPU)", "Pure Binary BW", "Classic Royal Blue (GPU)", "Classic Fire Red (GPU)",
    "Silver Crimson (GPU)"};

static void get_builtin_color(double fi, int pal, uint8_t* r, uint8_t* g, uint8_t* b) {
    double i_val = floor(fi);
    double fract = fi - i_val;
    double a[3], b_vec[3];

    for (int step = 0; step < 2; step++) {
        double iv = i_val + step;
        double* out = (step == 0) ? a : b_vec;

        if (pal == 0) {
            out[0] = (sin(0.1 * iv + 4.0) * 127.0 + 128.0) / 255.0;
            out[1] = (sin(0.1 * iv + 2.0) * 127.0 + 128.0) / 255.0;
            out[2] = (sin(0.1 * iv + 0.0) * 127.0 + 128.0) / 255.0;
        } else if (pal == 1) {
            // Volumetric Magma: realistic blackbody radiation curve
            double t = fmod(iv / 128.0, 1.0);
            out[0] = 1.0 - exp(-4.0 * t);
            out[1] = pow(t, 2.2);
            out[2] = pow(t, 7.0);
        } else if (pal == 2) {
            double t1 = 1.0 - fabs(fmod(iv / 256.0, 2.0) - 1.0);
            out[0] = 0.267 + t1 * (0.993 * t1 - 0.260);
            out[1] = 0.004 + t1 * (1.490 - t1 * 0.494);
            out[2] = 0.329 + t1 * (1.268 * t1 * t1 - 0.680 * t1 - 0.259);
        } else if (pal == 3) {
            // Grayscale
            out[0] = out[1] = out[2] = fmod(iv, 256.0) / 255.0;
        } else if (pal == 4) {
            // Electric
            out[0] = fmod(iv * 1.0, 256.0) / 255.0;
            out[1] = fmod(iv * 4.0, 256.0) / 255.0;
            out[2] = fmod(iv * 8.0, 256.0) / 255.0;
        } else if (pal == 5) {
            // Ocean
            out[0] = fmod(iv * 0.5, 256.0) / 255.0;
            out[1] = fmod(iv * 2.0, 256.0) / 255.0;
            out[2] = fmod(iv * 5.0, 256.0) / 255.0;
        } else if (pal == 6) {
            // Inferno
            out[0] = fmod(iv * 8.0, 256.0) / 255.0;
            out[1] = fmod(iv * 2.0, 256.0) / 255.0;
            out[2] = fmod(iv * 0.5, 256.0) / 255.0;
        } else if (pal == 7) {
            // retro binary: alternate green and blue
            if (fmod(iv, 2.0) < 1.0) {
                out[0] = 0.25; out[1] = 0.75; out[2] = 0.25; // green
            } else {
                out[0] = 0.1; out[1] = 0.2; out[2] = 0.8;    // blue
            }
        } else if (pal == 8) {
            // orbit mesh fallback: smooth cyan-to-navy gradient
            double t = fmod(iv / 128.0, 1.0);
            out[0] = (1.0 - t) * 0.0 + t * 0.02;
            out[1] = (1.0 - t) * 0.75 + t * 0.05;
            out[2] = (1.0 - t) * 0.85 + t * 0.25;
        } else if (pal == 9) {
            // biomorph trap fallback: smooth gold-to-dark-blue gradient
            double t = fmod(iv / 128.0, 1.0);
            out[0] = (1.0 - t) * 0.85 + t * 0.02;
            out[1] = (1.0 - t) * 0.65 + t * 0.12;
            out[2] = (1.0 - t) * 0.15 + t * 0.55;
        } else if (pal == 10) {
            // conformal ripples fallback: concentric ripple wave generator
            double rip = 0.5 + 0.5 * sin(iv * 0.35);
            out[0] = rip * 0.0;
            out[1] = rip * 0.65;
            out[2] = rip * 0.85;
        } else if (pal == 11) {
            // curvature marble fallback: psychedelic multi-frequency wave rings
            out[0] = 0.5 + 0.5 * sin(iv * 0.1 + 0.0);
            out[1] = 0.5 + 0.5 * sin(iv * 0.1 + 2.0);
            out[2] = 0.5 + 0.5 * sin(iv * 0.1 + 4.0);
        } else if (pal == 12) {
            // conformal grid fallback: smooth deep navy-to-bright-cyan gradient
            double t = fmod(iv / 128.0, 1.0);
            out[0] = (1.0 - t) * 0.0 + t * 0.01;
            out[1] = (1.0 - t) * 0.8 + t * 0.05;
            out[2] = (1.0 - t) * 0.95 + t * 0.35;
        } else if (pal == 13) {
            // cyber grid fallback: neon cyan-to-dark-green gradient
            double t = fmod(iv / 128.0, 1.0);
            out[0] = t * 0.0;
            out[1] = (1.0 - t) * 0.95 + t * 0.05;
            out[2] = (1.0 - t) * 0.6 + t * 0.15;
        } else if (pal == 14) {
            // bubble pearl fallback: blend teal and copper
            double t = fmod(iv / 100.0, 1.0);
            out[0] = (1.0 - t) * 0.1 + t * 0.85;
            out[1] = (1.0 - t) * 0.45 + t * 0.4;
            out[2] = (1.0 - t) * 0.55 + t * 0.2;
        } else if (pal == 15) {
            // liquid chrome fallback: iridescent silver reflection wave cycles
            out[0] = 0.6 + 0.4 * sin(iv * 0.15 + 1.0);
            out[1] = 0.65 + 0.35 * sin(iv * 0.15 + 2.0);
            out[2] = 0.7 + 0.3 * sin(iv * 0.15 + 3.0);
        } else if (pal == 16) {
            // refractive glass fallback: ice-blue to light purple wave gradient
            out[0] = 0.5 + 0.4 * sin(iv * 0.12 + 1.0);
            out[1] = 0.7 + 0.2 * sin(iv * 0.12 + 2.0);
            out[2] = 0.9 + 0.1 * sin(iv * 0.12 + 3.0);
        } else if (pal == 17) {
            // Ultra Fractal Classic
            double t = fmod(iv / 40.0, 1.0);
            if (t < 0.2) {
                double f = (t - 0.0) / 0.2;
                out[0] = (1.0 - f) * 0.0 + f * 1.0;
                out[1] = (1.0 - f) * 0.3 + f * 1.0;
                out[2] = (1.0 - f) * 0.7 + f * 1.0;
            } else if (t < 0.4) {
                double f = (t - 0.2) / 0.2;
                out[0] = (1.0 - f) * 1.0 + f * 1.0;
                out[1] = (1.0 - f) * 1.0 + f * 0.8;
                out[2] = (1.0 - f) * 1.0 + f * 0.0;
            } else if (t < 0.65) {
                double f = (t - 0.4) / 0.25;
                out[0] = (1.0 - f) * 1.0 + f * 0.5;
                out[1] = (1.0 - f) * 0.8 + f * 0.1;
                out[2] = (1.0 - f) * 0.0 + f * 0.05;
            } else if (t < 0.85) {
                double f = (t - 0.65) / 0.2;
                out[0] = (1.0 - f) * 0.5 + f * 0.0;
                out[1] = (1.0 - f) * 0.1 + f * 0.05;
                out[2] = (1.0 - f) * 0.05 + f * 0.3;
            } else {
                double f = (t - 0.85) / 0.15;
                out[0] = (1.0 - f) * 0.0 + f * 0.0;
                out[1] = (1.0 - f) * 0.05 + f * 0.3;
                out[2] = (1.0 - f) * 0.3 + f * 0.7;
            }
        } else if (pal == 18) {
            // Pure Binary BW
            out[0] = 1.0;
            out[1] = 1.0;
            out[2] = 1.0;
        } else if (pal == 19) {
            // Classic Royal Blue
            double t = fmod(iv / 60.0, 1.0);
            if (t < 0.3) {
                double f = (t - 0.0) / 0.3;
                out[0] = (1.0 - f) * 0.0 + f * 0.0;
                out[1] = (1.0 - f) * 0.02 + f * 0.1;
                out[2] = (1.0 - f) * 0.15 + f * 0.5;
            } else if (t < 0.75) {
                double f = (t - 0.3) / 0.45;
                out[0] = (1.0 - f) * 0.0 + f * 0.0;
                out[1] = (1.0 - f) * 0.1 + f * 0.8;
                out[2] = (1.0 - f) * 0.5 + f * 1.0;
            } else if (t < 0.95) {
                double f = (t - 0.75) / 0.2;
                out[0] = (1.0 - f) * 0.0 + f * 1.0;
                out[1] = (1.0 - f) * 0.8 + f * 1.0;
                out[2] = (1.0 - f) * 1.0 + f * 1.0;
            } else {
                double f = (t - 0.95) / 0.05;
                out[0] = (1.0 - f) * 1.0 + f * 0.0;
                out[1] = (1.0 - f) * 1.0 + f * 0.02;
                out[2] = (1.0 - f) * 1.0 + f * 0.15;
            }
        } else if (pal == 20) {
            // Classic Fire Red
            double t = fmod(iv / 50.0, 1.0);
            if (t < 0.35) {
                double f = (t - 0.0) / 0.35;
                out[0] = (1.0 - f) * 0.3 + f * 1.0;
                out[1] = (1.0 - f) * 0.0 + f * 0.0;
                out[2] = (1.0 - f) * 0.0 + f * 0.0;
            } else if (t < 0.75) {
                double f = (t - 0.35) / 0.40;
                out[0] = (1.0 - f) * 1.0 + f * 1.0;
                out[1] = (1.0 - f) * 0.0 + f * 0.7;
                out[2] = (1.0 - f) * 0.0 + f * 0.0;
            } else if (t < 0.95) {
                double f = (t - 0.75) / 0.20;
                out[0] = (1.0 - f) * 1.0 + f * 1.0;
                out[1] = (1.0 - f) * 0.7 + f * 1.0;
                out[2] = (1.0 - f) * 0.0 + f * 0.4;
            } else {
                double f = (t - 0.95) / 0.05;
                out[0] = (1.0 - f) * 1.0 + f * 0.3;
                out[1] = (1.0 - f) * 1.0 + f * 0.0;
                out[2] = (1.0 - f) * 0.4 + f * 0.0;
            }
        } else {
            // Silver Crimson
            double t = fmod(iv / 40.0, 1.0);
            double silver = 0.82 + 0.15 * sin(iv * 0.15);
            out[0] = silver;
            out[1] = silver;
            out[2] = silver;
            double red_factor = exp(-0.04 * (current_max_iterations - iv));
            if (red_factor > 1.0) red_factor = 1.0;
            if (red_factor < 0.0) red_factor = 0.0;
            out[0] = (1.0 - red_factor) * out[0] + red_factor * 0.9;
            out[1] = (1.0 - red_factor) * out[1] + red_factor * 0.1;
            out[2] = (1.0 - red_factor) * out[2] + red_factor * 0.05;
        }
    }

    *r = (uint8_t)((a[0] + (b_vec[0] - a[0]) * fract) * 255.0);
    *g = (uint8_t)((a[1] + (b_vec[1] - a[1]) * fract) * 255.0);
    *b = (uint8_t)((a[2] + (b_vec[2] - a[2]) * fract) * 255.0);
}

int get_palette_count(void) {
    return NUM_BUILTIN_PALETTES;
}

const char* get_palette_name(int idx) {
    if (idx < 0 || idx >= NUM_BUILTIN_PALETTES) return "Unknown";
    return builtin_palette_names[idx];
}

uint32_t* get_palette_lut(void) {
    return palette_lut_argb;
}

int get_palette_lut_size(void) {
    return lut_size;
}

int init_color_palette(int max_iterations, int palette_idx) {
    current_max_iterations = max_iterations;
    current_palette = palette_idx % NUM_BUILTIN_PALETTES;

    int needed_size = (max_iterations + 2) * 256;
    if (!palette_lut_argb || lut_size < needed_size) {
        /* allocate a new lut instead of realloc to prevent use-after-free 
           in background rendering threads. old luts are deferred for free. */
        uint32_t* new_lut = (uint32_t*)malloc(sizeof(uint32_t) * needed_size);
        if (new_lut) {
            if (palette_lut_argb) {
                if (num_old_luts < MAX_OLD_LUTS) {
                    old_luts[num_old_luts++] = palette_lut_argb;
                } else {
                    free(palette_lut_argb);
                }
            }
            palette_lut_argb = new_lut;
            lut_size = needed_size;
        } else {
            return 0;
        }
    }

    for (int fi = 0; fi < needed_size; fi++) {
        double iter_frac = (double)fi / 256.0;
        uint8_t r, g, b;
        get_builtin_color(iter_frac, current_palette, &r, &g, &b);
        palette_lut_argb[fi] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }
    return 1;
}

void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (iterations >= max_iterations || !palette_lut_argb) {
        *r = *g = *b = 0;
        return;
    }

    int idx = (int)(iterations * 256.0);
    if (idx < 0) idx = 0;
    if (idx >= lut_size) idx = lut_size - 1;

    uint32_t color = palette_lut_argb[idx];
    *b = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *r = color & 0xFF;
}

void cleanup_color_palette(void) {
    if (palette_lut_argb) {
        free(palette_lut_argb);
        palette_lut_argb = NULL;
    }
    for (int i = 0; i < num_old_luts; i++) {
        free(old_luts[i]);
    }
    num_old_luts = 0;
    lut_size = 0;
}
