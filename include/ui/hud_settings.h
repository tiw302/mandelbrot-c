/* hud_settings.h
 *
 * settings panel ui definitions.
 */
#ifndef HUD_SETTINGS_H
#define HUD_SETTINGS_H

#include "app_state.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

void hud_render_settings_window(AppCommonState* state, int win_w, int win_h, int gpu_mode,
                                int* high_precision_mode, int* use_perturbation);

#endif  // HUD_SETTINGS_H
