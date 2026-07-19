/* hud_video_studio.h
 *
 * video export studio ui definitions.
 */
#ifndef HUD_VIDEO_STUDIO_H
#define HUD_VIDEO_STUDIO_H

#include "app_state.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

void hud_render_video_studio(struct ImFont* custom_font, AppCommonState* state, int win_w,
                             int win_h, int* cpu_precision_128, uint32_t now);

#endif // HUD_VIDEO_STUDIO_H
