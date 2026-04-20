#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdint.h>

// save current pixel buffer to png using stb_image_write
void save_screenshot(uint32_t *pixels, int width, int height);

#endif
