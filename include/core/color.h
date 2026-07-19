/* color.h
 *
 * color palette definitions and blending functions.
 */

#ifndef CORE_COLOR_H
#define CORE_COLOR_H

#include <stdint.h>

/* initializes the color palette lookup table for the given palette index. * returns 1 on success, 0
 * on failure. */
int init_color_palette(int max_iterations, int palette_idx);

// gets the number of available built-in palettes
int get_palette_count(void);

// gets the name of the given palette index
const char* get_palette_name(int idx);

// gets the active pre-interpolated ARGB lookup table
uint32_t* get_palette_lut(void);

// gets the size of the lookup table
int get_palette_lut_size(void);

// calculates the smooth rgb color for a given fractional iteration count (legacy scalar)
void get_color(double iterations, int max_iterations, uint8_t* r, uint8_t* g, uint8_t* b);

// releases resources allocated for palette management
void cleanup_color_palette(void);

#endif // CORE_COLOR_H
