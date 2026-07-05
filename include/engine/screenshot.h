#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdint.h>

#include "core_math.h"

#include "renderer.h"
#include "app_state.h"

void save_screenshot(AppCommonState* state, const uint32_t* pixels, int width, int height, uint32_t now, int is_bgra, int is_bottom_up);

/*
 * captures a mega-resolution screenshot by rendering in horizontal strips
 * to save memory. saves directly to a raw tga file.
 */
int save_mega_screenshot(RendererContext* render_ctx, AppCommonState* state, int target_width, int target_height, precise_float re_min,
                          precise_float re_max, precise_float im_min, precise_float im_max,
                          int max_iterations, int palette_idx, int fractal_type, complex_t julia_c);

void save_mega_screenshot_async(RendererContext* render_ctx, AppCommonState* state, int target_width, int target_height, precise_float re_min,
                                precise_float re_max, precise_float im_min, precise_float im_max,
                                int max_iterations, int palette_idx, int fractal_type, complex_t julia_c);

// video recording api using ffmpeg via popen.
int start_video_recording(int width, int height, int fps, int is_bgra_topdown);
void append_video_frame(const uint32_t* pixels, int width, int height);
void stop_video_recording(void);
int is_video_recording(void);

#endif