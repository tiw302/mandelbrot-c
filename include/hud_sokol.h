#ifndef HUD_SOKOL_H
#define HUD_SOKOL_H

#include "app_state.h"

// forward declarations to avoid duplicate inclusion of sokol / fontstash headers
struct FONScontext;
#ifndef SGL_PIPELINE_DECLARED
#define SGL_PIPELINE_DECLARED
typedef struct sgl_pipeline sgl_pipeline;
#endif
struct sapp_event;

void hud_render_sokol_gpu(struct FONScontext* fons, int font_id, AppCommonState* state,
                          int win_w, int win_h, int gpu_mode, int high_precision_mode,
                          int cpu_precision_128, int active_perturbation_last,
                          int use_perturbation, sgl_pipeline pip_blend, uint32_t now);

void hud_render_sokol_deep(struct FONScontext* fons, int font_id, AppCommonState* state,
                           int win_w, int win_h, int high_precision_mode,
                           int active_perturbation_last, int use_perturbation,
                           sgl_pipeline pip_blend, uint32_t now);

void sokol_handle_mouse(AppCommonState* state, const struct sapp_event* event);
int sokol_handle_common_keydown(AppCommonState* state, const struct sapp_event* event, uint32_t now);

#endif
