#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <SDL2/SDL.h>

// save frame as PNG with timestamp
// returns 0 on success
int save_screenshot(SDL_Renderer *renderer, int width, int height);

#endif
