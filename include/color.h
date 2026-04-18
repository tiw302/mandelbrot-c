#ifndef CORE_COLOR_H
#define CORE_COLOR_H

#include <stdint.h>

#define PALETTE_COUNT 6

extern const char *PALETTE_NAMES[PALETTE_COUNT];

// Initialize the color Look-Up Table for smooth coloring
void init_color_palette(int max_iterations, int palette_idx);

// Map a fractal iteration count to its interpolated RGB representation
void get_color(double iterations, int max_iterations, uint8_t *r, uint8_t *g, uint8_t *b);

#endif // CORE_COLOR_H
