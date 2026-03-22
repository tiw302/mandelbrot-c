#ifndef JULIA_H
#define JULIA_H

#include "config.h"
#include "mandelbrot.h"  /* reuse complex_t */

/**
 * @brief Compute the Julia set escape iteration count for point z with parameter c.
 *
 * The key difference from the Mandelbrot set:
 *   - Mandelbrot: c varies per pixel, z always starts at 0.
 *   - Julia:      c is fixed (set by the user), z starts at each pixel's position.
 *
 * Iteration rule (same formula, different starting conditions):
 *   z_new = z^2 + c
 *
 * The Julia set J_c is the boundary of the set of z values that do NOT escape.
 * Every point in the Mandelbrot set produces a connected Julia set for its c value.
 *
 * @param z  Starting point in the complex plane (varies over the screen).
 * @param c  Fixed Julia parameter chosen by the user (mouse position on Mandelbrot).
 * @return   Iteration count before |z| > ESCAPE_RADIUS, or MAX_ITERATIONS if inside.
 */
int julia_check(complex_t z, complex_t c);

#endif /* JULIA_H */
