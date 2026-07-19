/* hud_sokol.h
 *
 * immediate-mode ui overlay for sokol backend.
 */
#ifndef HUD_SOKOL_H
#define HUD_SOKOL_H

#include "app_state.h"

// forward declarations to avoid duplicate inclusion of sokol / fontstash headers
struct ImFont;
struct sapp_event;

void hud_render_sokol_gpu(struct ImFont* custom_font, AppCommonState* state, int win_w, int win_h,
                          int gpu_mode, int* high_precision_mode, int cpu_precision_128,
                          int active_perturbation_last, int active_bignum, int* use_perturbation,
                          uint32_t now);

void hud_render_sokol_deep(struct ImFont* custom_font, AppCommonState* state, int win_w, int win_h,
                           int high_precision_mode, int active_perturbation_last, int active_bignum,
                           int use_perturbation, uint32_t now);
void hud_render_video_studio(struct ImFont* custom_font, AppCommonState* state, int win_w,
                             int win_h, int* cpu_precision_128, uint32_t now);

void sokol_handle_mouse(AppCommonState* state, const struct sapp_event* event);
int sokol_handle_common_keydown(AppCommonState* state, const struct sapp_event* event,
                                uint32_t now);

#endif  // HUD_SOKOL_H
