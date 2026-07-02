/* settings_panel.h
 *
 * interactive settings panel rendered via sokol_gl and fontstash.
 * provides sliders and buttons for real-time parameter adjustment.
 */

#ifndef SETTINGS_PANEL_H
#define SETTINGS_PANEL_H

#include <stdint.h>
#include "app_state.h"

// forward declarations to avoid duplicate inclusion of sokol / fontstash headers
struct FONScontext;
#ifndef SGL_PIPELINE_DECLARED
#define SGL_PIPELINE_DECLARED
typedef struct sgl_pipeline sgl_pipeline;
#endif

// panel width in pixels
#define SETTINGS_PANEL_WIDTH 240

// internal drag state for the iterations slider
typedef struct {
    int  visible;          // 1 = panel is open
    int  drag_active;      // 1 = user is dragging the slider
    int  drag_start_x;     // mouse x when drag started
    int  drag_start_iter;  // iteration value when drag started
} SettingsPanel;

// renders the settings panel overlay on top of the fractal
void settings_panel_render(SettingsPanel* panel, struct FONScontext* fons, int font_id,
                            AppCommonState* state, int win_w, int win_h,
                            sgl_pipeline pip_blend, uint32_t now);

// processes a mouse event; returns 1 if the event was consumed by the panel
int settings_panel_handle_mouse_down(SettingsPanel* panel, AppCommonState* state,
                                      int mx, int my, int win_w, int win_h);
int settings_panel_handle_mouse_move(SettingsPanel* panel, AppCommonState* state, int mx);
int settings_panel_handle_mouse_up  (SettingsPanel* panel, AppCommonState* state);

#endif // SETTINGS_PANEL_H
