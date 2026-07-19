/* hud_sokol.c
 *
 * immediate-mode ui overlay using cimgui (c bindings for dear imgui).
 * rebuilt every frame from AppCommonState — no stored ui object state.
 * the ui layer only reads from the state pointer and dispatches events back,
 * keeping the core fractal engine ui-agnostic.
 */

#include "hud_sokol.h"

#include <stdio.h>
#include <string.h>

#include "color.h"
#include "config.h"
#include "config_loader.h"
#include "fractal.h"
#include "hud_settings.h"
#include "hud_video_studio.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

void hud_render_sokol_gpu(struct ImFont* custom_font, AppCommonState* state, int win_w, int win_h,
                          int gpu_mode, int* high_precision_mode, int cpu_precision_128,
                          int active_perturbation_last, int active_bignum, int* use_perturbation,
                          uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }

    ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    if (state->show_help) {
        igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always,
                           (ImVec2){0.5f, 0.5f});
        igSetNextWindowBgAlpha(0.9f);
        if (igBegin("Help", NULL, overlay_flags)) {
            igTextColored((ImVec4){1.0f, 0.2f, 0.2f, 1.0f}, "[ Keyboard Controls Guide ]");
            igSeparator();
            igText("H / F5       : Toggle Help / Reload Shaders");
            igText("ESC / Q      : Quit Application");
            igText("Ctrl + Z     : Undo Camera Zoom/Pan");
            igText("R            : Reset View");
            igText("P / 0-9      : Cycle / Select Color Palette");
            igText("UP / DOWN    : Adjust Iterations (Shift x10)");
            igText("E            : Toggle 64-bit GPU Emulation (Dekker)");
            igText("J            : Toggle Julia Explorer Mode");
            igText("K            : Lock / Unlock Julia Shape");
            igText("B            : Toggle Burning Ship Mode");
            igText("N            : Toggle Perturbation Theory Mode");
            igText("S            : Capture Screenshot");
            igText("V            : Start / Stop Video Recording");
            igEnd();
        }
    }

    igSetNextWindowPos((ImVec2){15.0f, 15.0f}, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
    igSetNextWindowBgAlpha(0.3f);
    if (igBegin("HUD", NULL, overlay_flags)) {
        const char* engine_name = "CPU";
        const char* precision_tier = "Double (64-bit)";

        if (gpu_mode) {
            engine_name = "GPU";
#ifdef BUILD_PERTURBATION
            if (active_bignum) {
                precision_tier = "Perturbation (BigNum 1024-bit)";
            } else if (active_perturbation_last) {
                precision_tier =
                    *high_precision_mode ? "Perturbation (double-64)" : "Perturbation (double-64)";
            } else if (*use_perturbation && (state->cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD) &&
                       !state->julia_mode && !state->base_fractal) {
                if ((float)state->cam.view.zoom == 0.0f) {
                    precision_tier = "Perturbation (double-64)";
                } else {
                    precision_tier =
                        *high_precision_mode ? "Double (64-bit emulation)" : "Double (32-bit)";
                }
            } else {
                precision_tier =
                    *high_precision_mode ? "Double (64-bit emulation)" : "Double (32-bit)";
            }
#else
            precision_tier = *high_precision_mode ? "Double (64-bit emulation)" : "Double (32-bit)";
#endif
        } else {
#ifdef USE_SIMD_F128
            if (cpu_precision_128) {
                precision_tier = "Double-double (128-bit)";
            } else {
                precision_tier = "Double (64-bit)";
            }
#endif
        }
        const FractalDefinition* fd = get_fractal_by_mode(state->base_fractal);
        igText("[ENGINE]    %s | Mode: %s | Threads: %d | Render: %u ms", engine_name,
               state->julia_mode ? "Julia" : (fd ? fd->display_name : "Unknown"),
               state->thread_count, state->render_time_ms);

        /* color-code the precision tier: green for standard, yellow for perturbation double,
         * cyan for bignum — gives instant visual feedback on which backend is running. */
        ImVec4 tier_color = {0.4f, 1.0f, 0.4f, 1.0f};  // green: standard double
        if (active_bignum) {
            tier_color = (ImVec4){0.2f, 0.9f, 1.0f, 1.0f};  // cyan: BigNum
        } else if (active_perturbation_last) {
            tier_color = (ImVec4){1.0f, 0.85f, 0.2f, 1.0f};  // yellow: perturbation double
        }
        igText("[PRECISION] ");
        igSameLine(0.0f, 0.0f);
        igTextColored(tier_color, "%s", precision_tier);

        if (state->julia_mode) {
            igText("[COORD]     C: (%.14f, %.14f)", state->julia_c.re, state->julia_c.im);
        } else {
            igText("[COORD]     Center: (%.14f, %.14f)", (double)state->cam.view.center_re,
                   (double)state->cam.view.center_im);
        }

        igText("[RENDER]    Zoom: %.6g | Iters: %d | Palette: %s", (double)state->cam.view.zoom,
               state->max_iterations, get_palette_name(state->palette_idx % get_palette_count()));

        // tour text removed as requested for deep zoom
        igEnd();
    }

    int draw_count = 0;
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            uint32_t elapsed = now - state->notifications[i].start_time;
            if (elapsed > 3000) {
                state->notifications[i].active = 0;
            } else {
                char window_name[32];
                snprintf(window_name, sizeof(window_name), "Notif_%d", i);
                float try_y = (float)win_h - 20.0f - (float)draw_count * 45.0f;
                igSetNextWindowPos((ImVec2){(float)win_w - 20.0f, try_y}, ImGuiCond_Always,
                                   (ImVec2){1.0f, 1.0f});
                igSetNextWindowBgAlpha(0.7f);
                if (igBegin(window_name, NULL, overlay_flags)) {
                    igText("%s", state->notifications[i].message);
                    igEnd();
                }
                draw_count++;
            }
        }
    }

    if (state->mega_screenshot_active == 1) {
        igSetNextWindowPos((ImVec2){20.0f, (float)win_h - 20.0f}, ImGuiCond_Always,
                           (ImVec2){0.0f, 1.0f});
        igSetNextWindowBgAlpha(0.7f);
        if (igBegin("MegaScreenshot", NULL, overlay_flags)) {
            igText("Generating 8K: %d%%", state->mega_screenshot_progress);
            igProgressBar((float)state->mega_screenshot_progress / 100.0f, (ImVec2){200.0f, 0.0f},
                          NULL);
            igEnd();
        }
    }

    if (custom_font) {
        if (state->show_settings) {
            hud_render_settings_window(state, win_w, win_h, gpu_mode, high_precision_mode,
                                       use_perturbation);
        }

        igPopFont();
    }
}

void hud_render_sokol_deep(struct ImFont* custom_font, AppCommonState* state, int win_w, int win_h,
                           int high_precision_mode, int active_perturbation_last, int active_bignum,
                           int use_perturbation, uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }

    ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (state->show_help) {
        igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always,
                           (ImVec2){0.5f, 0.5f});
        igSetNextWindowBgAlpha(0.9f);
        if (igBegin("Help", NULL, overlay_flags)) {
            igTextColored((ImVec4){1.0f, 0.2f, 0.2f, 1.0f}, "[ Keyboard Controls Guide ]");
            igSeparator();
            igText("H / F5       : Toggle Help / Reload Shaders");
            igText("ESC / Q      : Quit Application");
            igText("Ctrl + Z     : Undo Camera Zoom/Pan");
            igText("R            : Reset View");
            igText("P / 0-9      : Cycle / Select Color Palette");
            igText("UP / DOWN    : Adjust Iterations (Shift x10)");
            igText("E            : Toggle 64-bit GPU Emulation (Dekker)");
            igText("N            : Toggle Perturbation Theory Mode");
            igText("S            : Capture Screenshot");
            igEnd();
        }
    } else {
        igSetNextWindowPos((ImVec2){15.0f, 15.0f}, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
        igSetNextWindowBgAlpha(0.3f);
        if (igBegin("HUD", NULL, overlay_flags)) {
            const char* engine_name = "GPU";
            const char* precision_tier = "Double (32-bit)";
            if (active_bignum) {
                precision_tier = "Perturbation (BigNum 1024-bit)";
            } else if (active_perturbation_last) {
                precision_tier = "Perturbation (double-64)";
            } else if (use_perturbation && (state->cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD)) {
                if ((float)state->cam.view.zoom == 0.0f) {
                    precision_tier = "Perturbation (double-64)";
                } else {
                    precision_tier =
                        high_precision_mode ? "Double (64-bit emulation)" : "Double (32-bit)";
                }
            } else {
                precision_tier =
                    high_precision_mode ? "Double (64-bit emulation)" : "Double (32-bit)";
            }

            igText("[ENGINE]    %s | Mode: Mandelbrot (Deep Zoom Mode)", engine_name);

            ImVec4 tier_color = {0.4f, 1.0f, 0.4f, 1.0f};  // green: standard double
            if (active_bignum) {
                tier_color = (ImVec4){0.2f, 0.9f, 1.0f, 1.0f};  // cyan: BigNum
            } else if (active_perturbation_last) {
                tier_color = (ImVec4){1.0f, 0.85f, 0.2f, 1.0f};  // yellow: perturbation double
            }
            igText("[PRECISION] ");
            igSameLine(0.0f, 0.0f);
            igTextColored(tier_color, "%s", precision_tier);

            igText("[COORD]     Center: (%.14f, %.14f)", (double)state->cam.view.center_re,
                   (double)state->cam.view.center_im);
            igText("[RENDER]    Zoom: %.6g | Iters: %d | Palette: %s", (double)state->cam.view.zoom,
                   state->max_iterations,
                   get_palette_name(state->palette_idx % get_palette_count()));
            igEnd();
        }
    }

    int draw_count = 0;
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            uint32_t elapsed = now - state->notifications[i].start_time;
            if (elapsed > 3000) {
                state->notifications[i].active = 0;
            } else {
                char window_name[32];
                snprintf(window_name, sizeof(window_name), "Notif_%d", i);
                float try_y = (float)win_h - 20.0f - (float)draw_count * 45.0f;
                igSetNextWindowPos((ImVec2){(float)win_w - 20.0f, try_y}, ImGuiCond_Always,
                                   (ImVec2){1.0f, 1.0f});
                igSetNextWindowBgAlpha(0.7f);
                if (igBegin(window_name, NULL, overlay_flags)) {
                    igText("%s", state->notifications[i].message);
                    igEnd();
                }
                draw_count++;
            }
        }
    }

    if (custom_font) {
        if (state->show_settings) {
            hud_render_settings_window(state, win_w, win_h, 1, &high_precision_mode,
                                       &use_perturbation);
        }

        igPopFont();
    }
}
