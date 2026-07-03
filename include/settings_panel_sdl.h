/* settings_panel_sdl.h
 *
 * interactive settings panel for the SDL2 CPU-mode renderer.
 * draws over the fractal view using SDL_RenderFillRect + TTF_Font text.
 */

#ifndef SETTINGS_PANEL_SDL_H
#define SETTINGS_PANEL_SDL_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include "app_state.h"

// panel width in pixels (matches GPU panel for visual consistency)
#define SDL_PANEL_WIDTH 260

// action returned from mouse_down so the backend can react
typedef enum {
    SDL_PANEL_ACTION_NONE = 0,
    SDL_PANEL_ACTION_CONSUMED,        // click consumed, no backend action needed
    SDL_PANEL_ACTION_TOGGLE_PRECISION, // user clicked precision button
    SDL_PANEL_ACTION_THREADS_UP,       // user clicked thread + button
    SDL_PANEL_ACTION_THREADS_DOWN,     // user clicked thread - button
} SdlPanelAction;

// internal drag state for the iterations slider
typedef struct {
    int  visible;
    int  drag_active;
    int  drag_start_x;
    int  drag_start_iter;
} SettingsPanelSdl;

// render the panel on top of the fractal using the SDL renderer
// cpu_precision_128: 1 = 128-bit, 0 = 64-bit
// thread_count: current active thread count
void settings_panel_sdl_render(SettingsPanelSdl* panel, SDL_Renderer* renderer,
                                TTF_Font* font, AppCommonState* state,
                                int win_w, int win_h,
                                int cpu_precision_128, int thread_count,
                                uint32_t now);

// mouse event handlers; return SdlPanelAction
SdlPanelAction settings_panel_sdl_mouse_down(SettingsPanelSdl* panel, AppCommonState* state,
                                              int mx, int my, int win_w, int win_h);
int settings_panel_sdl_mouse_move(SettingsPanelSdl* panel, AppCommonState* state, int mx);
int settings_panel_sdl_mouse_up  (SettingsPanelSdl* panel, AppCommonState* state);

#endif // SETTINGS_PANEL_SDL_H
