#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include "config.h"
#include "mandelbrot.h"

// Structure to pass data to each rendering thread
typedef struct {
    int id;                // Thread ID
    Uint32* pixels;        // Pointer to the pixel buffer
    int pitch;             // Pitch of the pixel buffer
    int start_y;           // Starting Y coordinate for this thread
    int end_y;             // Ending Y coordinate for this thread
    int window_width;      // Current width of the window
    int window_height;     // Current height of the window
    double re_min, re_max; // Real axis bounds
    double im_min, im_max; // Imaginary axis bounds
} thread_data_t;

/**
 * @brief Function executed by each rendering thread.
 *
 * @param arg A pointer to thread_data_t containing rendering parameters.
 * @return NULL
 */
void* render_thread(void* arg);

/**
 * @brief Renders the Mandelbrot set using multiple threads.
 *
 * @param pixels Pointer to the pixel buffer to write to.
 * @param pitch Pitch of the pixel buffer.
 * @param window_width The current width of the window.
 * @param window_height The current height of the window.
 * @param re_min Minimum real value of the complex plane.
 * @param re_max Maximum real value of the complex plane.
 * @param im_min Minimum imaginary value of the complex plane.
 * @param im_max Maximum imaginary value of the complex plane.
 */
void render_mandelbrot_threaded(Uint32* pixels, int pitch,
                                 int window_width, int window_height,
                                 double re_min, double re_max,
                                 double im_min, double im_max);


// Function to map iterations to a color (black to red gradient)
void get_color(int iterations, Uint8* r, Uint8* g, Uint8* b);

#endif // RENDERER_H
