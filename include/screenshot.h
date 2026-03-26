#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <SDL2/SDL.h>

/**
 * Saves the current frame to a timestamped PNG file.
 * Returns 0 on success, non-zero on failure.
 */
int save_screenshot(SDL_Renderer *renderer, int width, int height);

#endif
