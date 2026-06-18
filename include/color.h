#ifndef CORE_COLOR_H
#define CORE_COLOR_H

#include <stdint.h>

// total count of built-in color palettes
#define PALETTE_COUNT 9

// human-readable names for palette selection ui
extern const char* PALETTE_NAMES[PALETTE_COUNT];

// prepares the color look-up table or interpolation state.
// returns 1 on success, 0 on memory allocation failure.
int init_color_palette(int max_iterations, int palette_idx);

// calculates the smooth rgb color for a given fractional iteration count
void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b);

// releases resources allocated for palette management
void cleanup_color_palette(void);

#endif
