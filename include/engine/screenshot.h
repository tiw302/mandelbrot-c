/* screenshot.h
 *
 * async high-resolution screenshot and video export.
 */
#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdint.h>

#include "app_state.h"
#include "core_math.h"
#include "renderer.h"

void save_screenshot(AppCommonState* state, const uint32_t* pixels, int width, int height,
                     uint32_t now, int is_bgra, int is_bottom_up);

/*
 * captures a mega-resolution screenshot by rendering in horizontal strips
 * to save memory. saves directly to a raw tga file.
 */
int save_mega_screenshot(RendererContext* render_ctx, AppCommonState* state, int target_width,
                         int target_height, precise_float re_min, precise_float re_max,
                         precise_float im_min, precise_float im_max, int max_iterations,
                         int fractal_type, complex_t julia_c);

void save_mega_screenshot_async(RendererContext* render_ctx, AppCommonState* state,
                                int target_width, int target_height, precise_float re_min,
                                precise_float re_max, precise_float im_min, precise_float im_max,
                                int max_iterations, int fractal_type, complex_t julia_c);

// video recording api using ffmpeg via popen.
int start_video_recording(int width, int height, int fps, int is_bgra_topdown, int crf,
                          const char* preset, const char* codec, int aa_level, int show_log,
                          const char* log_fontpath, int log_fontsize, const char* custom_filename,
                          int log_position, const char* log_fontcolor);

// async background video export
void start_video_export_async(AppCommonState* state);
void append_video_frame(const uint32_t* pixels, int width, int height);
void stop_video_recording(void);
int is_video_recording(void);

#endif
