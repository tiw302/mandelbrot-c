#pragma once

#include "app_state.h"

#if defined(__EMSCRIPTEN__)
void wasm_bridge_init(AppCommonState* state);

void call_update_debug_info(int gpu_mode, int julia_mode, int base_fractal, int max_iters,
                            double zoom, double center_re, double center_im, int palette_idx,
                            int tour_phase, double julia_re, double julia_im, int high_precision,
                            int tour_target_idx, int tour_total_targets, double tour_target_re,
                            double tour_target_im, int thread_count, int render_time_ms);
#endif
