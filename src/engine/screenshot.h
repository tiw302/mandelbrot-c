#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdint.h>
#include "core_math.h"

void save_screenshot(uint32_t* pixels, int width, int height);

/* 
 * captures a mega-resolution screenshot by rendering in horizontal strips 
 * to save memory. saves directly to a raw tga file.
 */
void save_mega_screenshot(int target_width, int target_height, double re_min, double re_max, 
                          double im_min, double im_max, int max_iterations, int palette_idx, 
                          int fractal_type, complex_t julia_c);

/* 
 * video recording api using ffmpeg via popen.
 */
int start_video_recording(int width, int height, int fps);
void append_video_frame(uint32_t* pixels, int width, int height);
void stop_video_recording(void);
int is_video_recording(void);

#endif