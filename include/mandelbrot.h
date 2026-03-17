#ifndef MANDELBROT_H
#define MANDELBROT_H

#include "config.h" // For MAX_ITERATIONS

// Structure to represent a complex number
typedef struct {
    double re; // Real part
    double im; // Imaginary part
} complex_t;

/**
 * @brief Checks if a given complex number belongs to the Mandelbrot set.
 *
 * This function calculates the number of iterations required for the magnitude
 * of z to exceed ESCAPE_RADIUS for a given complex number c.
 *
 * @param c The complex number to test.
 * @return The number of iterations (0 to MAX_ITERATIONS). If it reaches MAX_ITERATIONS,
 *         it's considered to be in the set.
 */
int mandelbrot_check(complex_t c);

#endif // MANDELBROT_H
