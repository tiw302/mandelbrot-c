#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <SDL2/SDL.h>

/**
 * @brief Save the current rendered frame as a PNG file.
 *
 * The filename is auto-generated with a timestamp:
 *   mandelbrot_YYYYMMDD_HHMMSS.png
 *
 * Implementation uses zlib (already a transitive dependency of SDL2) to
 * compress the pixel data -- no extra libraries required.
 *
 * @param renderer  Active SDL renderer to read pixels from.
 * @param width     Viewport width in pixels.
 * @param height    Viewport height in pixels.
 */
void save_screenshot(SDL_Renderer *renderer, int width, int height);

#endif /* SCREENSHOT_H */
