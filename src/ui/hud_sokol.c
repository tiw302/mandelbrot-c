/* hud_sokol.c
 *
 * telemetry and notification overlay rendering using cimgui.
 * processes mouse events.
 */

#include "hud_sokol.h"
#include "color.h"
#include "config.h"
#include "config_loader.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"
#include "fractal.h"
#include <stdio.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

void hud_render_sokol_gpu(struct ImFont* custom_font, AppCommonState* state,
                          int win_w, int win_h, int gpu_mode, int* high_precision_mode,
                          int cpu_precision_128, int active_perturbation_last,
                          int* use_perturbation, uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (state->show_help) {
        igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always, (ImVec2){0.5f, 0.5f});
        igSetNextWindowBgAlpha(0.9f);
        if (igBegin("Help", NULL, flags)) {
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
            igText("B            : Toggle Burning Ship Mode");
            igText("N            : Toggle Perturbation Theory Mode");
            igText("S            : Capture Screenshot");
            igText("V            : Start / Stop Video Recording");
            igEnd();
        }
    } else {
        igSetNextWindowPos((ImVec2){15.0f, 15.0f}, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
        igSetNextWindowBgAlpha(0.3f);
        if (igBegin("HUD", NULL, flags)) {
            const char* engine_name = "CPU (64-bit)";
            if (gpu_mode) {
#ifdef BUILD_PERTURBATION
                if (active_perturbation_last) {
                    engine_name = "GPU (Perturbation)";
                } else if (*use_perturbation &&
                           (state->cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD) &&
                           !state->julia_mode && !state->base_fractal) {
                    if ((float)state->cam.view.zoom == 0.0f) {
                        engine_name = "GPU (Perturbation)";
                    } else {
                        engine_name = "GPU (32-bit)";
                    }
                } else {
                    engine_name = *high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
                }
#else
                engine_name = *high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
#endif
            } else {
#ifdef USE_SIMD_F128
                engine_name = cpu_precision_128 ? "CPU (128-bit)" : "CPU (64-bit)";
#endif
            }
            const FractalDefinition* fd = get_fractal_by_mode(state->base_fractal);
            igText("[ENGINE] %s | Mode: %s | Threads: %d | Render: %u ms",
                     engine_name,
                     state->julia_mode ? "Julia" : (fd ? fd->display_name : "Unknown"),
                     state->thread_count, state->render_time_ms);
            
            if (state->julia_mode) {
                igText("[COORD]  C: (%.14f, %.14f)", state->julia_c.re, state->julia_c.im);
            } else {
                igText("[COORD]  Center: (%.14f, %.14f)", (double)state->cam.view.center_re, (double)state->cam.view.center_im);
            }
            
            igText("[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
                     (double)state->cam.view.zoom, state->max_iterations,
                     get_palette_name(state->palette_idx % get_palette_count()));
                     
            if (state->m_tour.phase != TOUR_IDLE) {
                int t_idx = get_tour_target_idx(&state->m_tour);
                int t_tot = get_num_tour_targets(state->base_fractal);
                double t_re = get_tour_target_re(&state->m_tour, state->base_fractal);
                double t_im = get_tour_target_im(&state->m_tour, state->base_fractal);
                igText("[TOUR]   Auto-Zoom [%s] Target #%d/%d (%.4f, %.4f)",
                         get_tour_phase_name(state->m_tour.phase), t_idx + 1, t_tot, t_re, t_im);
            }
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
                igSetNextWindowPos((ImVec2){(float)win_w - 20.0f, try_y}, ImGuiCond_Always, (ImVec2){1.0f, 1.0f});
                igSetNextWindowBgAlpha(0.7f);
                if (igBegin(window_name, NULL, flags)) {
                    igText("%s", state->notifications[i].message);
                    igEnd();
                }
                draw_count++;
            }
        }
    }

    if (state->mega_screenshot_active == 1) {
        igSetNextWindowPos((ImVec2){20.0f, (float)win_h - 20.0f}, ImGuiCond_Always, (ImVec2){0.0f, 1.0f});
        igSetNextWindowBgAlpha(0.7f);
        if (igBegin("MegaScreenshot", NULL, flags)) {
            igText("Generating 8K: %d%%", state->mega_screenshot_progress);
            // Draw progress bar using ImGui
            igProgressBar((float)state->mega_screenshot_progress / 100.0f, (ImVec2){200.0f, 0.0f}, NULL);
            igEnd();
        }
    }

    if (custom_font) {
        if (state->show_settings) {
            igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always, (ImVec2){0.5f, 0.5f});
            igSetNextWindowBgAlpha(0.9f);
            if (igBegin("Settings", NULL, flags)) {
                igTextColored((ImVec4){1.0f, 0.2f, 0.2f, 1.0f}, "[ Engine Settings ]");
                igSeparator();
                
                // Max Iterations
                int max_iter = state->max_iterations;
                if (igSliderInt("Max Iterations", &max_iter, 100, 100000, "%d", ImGuiSliderFlags_Logarithmic)) {
                    state->max_iterations = max_iter;
                    state->needs_redraw = 1;
                }
                
                // CPU Threads
                if (!gpu_mode) {
                    int threads = state->thread_count;
                    if (igSliderInt("CPU Threads", &threads, 1, 64, "%d", ImGuiSliderFlags_None)) {
                        state->thread_count = threads;
                        state->needs_redraw = 1;
                    }
                }
                
                // Palettes
                const char* palette_names[32];
                int pal_count = get_palette_count();
                if (pal_count > 32) pal_count = 32;
                for (int i = 0; i < pal_count; i++) {
                    palette_names[i] = get_palette_name(i);
                }
                
                int current_pal = state->palette_idx;
                if (igCombo_Str_arr("Color Palette", &current_pal, palette_names, pal_count, -1)) {
                    state->palette_idx = current_pal;
                    init_color_palette(state->max_iterations, current_pal);
                    state->needs_redraw = 1;
                }

                // Fractal Types
                const char* fractal_names[] = {
                    "Mandelbrot", "Burning Ship", "Tricorn", "Celtic", "Buffalo"
                };
                int current_frac = state->base_fractal;
                if (igCombo_Str_arr("Fractal Type", &current_frac, fractal_names, 5, -1)) {
                    state->base_fractal = current_frac;
                    state->needs_redraw = 1;
                }

                // Precision Checkboxes
                bool hp = *high_precision_mode;
                if (igCheckbox("High Precision Mode", &hp)) {
                    *high_precision_mode = hp;
                    state->needs_redraw = 1;
                }
                
                if (gpu_mode) {
                    bool pert = *use_perturbation;
                    if (igCheckbox("Deep Zoom (Perturbation)", &pert)) {
                        *use_perturbation = pert;
                        state->needs_redraw = 1;
                    }
                }
                
                igSeparator();
                igTextColored((ImVec4){1.0f, 0.2f, 0.2f, 1.0f}, "[ Bookmarks & Logs ]");
                igSeparator();
                
                static char new_bookmark_name[64] = "";
                igSetNextItemWidth(150.0f);
                igInputTextWithHint("##NewBookmarkName", "Bookmark name...", new_bookmark_name, sizeof(new_bookmark_name), ImGuiInputTextFlags_None, NULL, NULL);
                igSameLine(0, -1);
                if (igButton("Save Bookmark", (ImVec2){-1, 0})) {
                    app_state_save_bookmark_with_name(state, new_bookmark_name[0] != '\0' ? new_bookmark_name : NULL);
                    new_bookmark_name[0] = '\0';
                }
                
                int count;
                const Bookmark* bookmarks = app_state_get_bookmarks_array(&count);
                if (count > 0 && bookmarks) {
                    igText("Saved Bookmarks (%d):", count);
                    if (igBeginChild_Str("BookmarksList", (ImVec2){0, 150}, true, ImGuiWindowFlags_None)) {
                        for (int b = 0; b < count; b++) {
                            igPushID_Int(b);
                            
                            if (igButton("X", (ImVec2){0, 0})) {
                                app_state_delete_bookmark(state, b);
                            }
                            igSameLine(0, -1);
                            
                            char load_label[128];
                            snprintf(load_label, sizeof(load_label), "Load##%d", b);
                            if (igButton(load_label, (ImVec2){0, 0})) {
                                app_state_load_bookmark(state, b);
                            }
                            igSameLine(0, -1);
                            
                            igTextColored((ImVec4){0.8f, 0.8f, 0.8f, 1.0f}, "%s", bookmarks[b].name);
                            igSameLine(0, -1);
                            igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "(Re: %.3g, Im: %.3g, Z: %.1e)", 
                                          bookmarks[b].center_re, bookmarks[b].center_im, bookmarks[b].zoom);
                                          
                            igPopID();
                        }
                    }
                    igEndChild();
                } else {
                    igTextColored((ImVec4){0.6f, 0.6f, 0.6f, 1.0f}, "No bookmarks saved yet.");
                }
                
                igSeparator();
                igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "Press TAB to close");
            }
            igEnd();
        }

        igPopFont();
    }
}

void hud_render_sokol_deep(struct ImFont* custom_font, AppCommonState* state,
                           int win_w, int win_h, int high_precision_mode,
                           int active_perturbation_last, int use_perturbation,
                           uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (state->show_help) {
        igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always, (ImVec2){0.5f, 0.5f});
        igSetNextWindowBgAlpha(0.9f);
        if (igBegin("Help", NULL, flags)) {
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
        if (igBegin("HUD", NULL, flags)) {
            const char* engine_name = "GPU (32-bit)";
            if (active_perturbation_last) {
                engine_name = "GPU (Perturbation)";
            } else if (use_perturbation && (state->cam.view.zoom < PERTURBATION_ZOOM_THRESHOLD)) {
                if ((float)state->cam.view.zoom == 0.0f) {
                    engine_name = "GPU (Perturbation: No Float!)";
                } else {
                    engine_name = "GPU (Perturbation: Fallback)";
                }
            } else {
                engine_name = high_precision_mode ? "GPU (64-bit emulation)" : "GPU (32-bit)";
            }

            igText("[ENGINE] %s | Mode: Mandelbrot (Deep Zoom Mode)", engine_name);
            igText("[COORD]  Center: (%.14f, %.14f)", (double)state->cam.view.center_re, (double)state->cam.view.center_im);
            igText("[RENDER] Zoom: %.6g | Iters: %d | Palette: %s",
                     (double)state->cam.view.zoom, state->max_iterations,
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
                igSetNextWindowPos((ImVec2){(float)win_w - 20.0f, try_y}, ImGuiCond_Always, (ImVec2){1.0f, 1.0f});
                igSetNextWindowBgAlpha(0.7f);
                if (igBegin(window_name, NULL, flags)) {
                    igText("%s", state->notifications[i].message);
                    igEnd();
                }
                draw_count++;
            }
        }
    }

    if (custom_font) {

        igPopFont();
    }
}

void hud_render_video_studio(struct ImFont* custom_font, AppCommonState* state,
                             int win_w, int win_h, uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always, (ImVec2){0.5f, 0.5f});
    igSetNextWindowBgAlpha(0.9f);

    if (igBegin("Video Export Studio", NULL, flags)) {
        igTextColored((ImVec4){0.2f, 0.8f, 1.0f, 1.0f}, "[ Video Export Studio ]");
        igSeparator();

        if (state->video_is_rendering) {
            igText("Rendering in progress...");
            float progress = 0.0f;
            if (state->m_tour.phase != 0 && state->video_duration_sec > 0) {
                progress = (float)(now - state->m_tour.phase_start) / (state->video_duration_sec * 1000.0f);
                if (progress > 1.0f) progress = 1.0f;
            }
            igProgressBar(progress, (ImVec2){300.0f, 0.0f}, NULL);

            igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.8f, 0.1f, 0.1f, 1.0f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){1.0f, 0.2f, 0.2f, 1.0f});
            if (igButton("Cancel Render", (ImVec2){-1, 0})) {
                state->video_is_rendering = 0;
                app_state_toggle_tour(state, now, NULL); // Stop tour
                if (is_video_recording()) {
                    stop_video_recording();
                    app_state_push_notification(state, "Render Cancelled!", now);
                }
            }
            igPopStyleColor(2);
        } else {
            // Resolution setup
            igText("Output Resolution:");
            igSetNextItemWidth(100.0f);
            igInputInt("Width", &state->video_res_w, 1, 100, ImGuiInputTextFlags_None);
            igSameLine(0, -1);
            igSetNextItemWidth(100.0f);
            igInputInt("Height", &state->video_res_h, 1, 100, ImGuiInputTextFlags_None);

            // FPS and Duration
            igSetNextItemWidth(100.0f);
            igSliderInt("FPS", &state->video_fps, 24, 120, "%d", ImGuiSliderFlags_None);
            igSetNextItemWidth(100.0f);
            igSliderInt("Duration (s)", &state->video_duration_sec, 1, 300, "%d", ImGuiSliderFlags_None);

            igSeparator();
            igText("Target Destination:");
            
            // Load target from bookmark
            int count;
            const Bookmark* bookmarks = app_state_get_bookmarks_array(&count);
            if (count > 0 && bookmarks) {
                if (igBeginCombo("Load from Bookmark", "Select...", ImGuiComboFlags_None)) {
                    for (int b = 0; b < count; b++) {
                        igPushID_Int(b);
                        if (igSelectable_Bool(bookmarks[b].name, false, ImGuiSelectableFlags_None, (ImVec2){0,0})) {
                            state->video_target_re = bookmarks[b].center_re;
                            state->video_target_im = bookmarks[b].center_im;
                            state->video_target_zoom = bookmarks[b].zoom;
                        }
                        igPopID();
                    }
                    igEndCombo();
                }
            } else {
                igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "(No bookmarks available)");
            }

            igSetNextItemWidth(250.0f);
            igInputDouble("Target Re", &state->video_target_re, 0.0, 0.0, "%.15f", ImGuiInputTextFlags_None);
            igSetNextItemWidth(250.0f);
            igInputDouble("Target Im", &state->video_target_im, 0.0, 0.0, "%.15f", ImGuiInputTextFlags_None);
            igSetNextItemWidth(250.0f);
            
            // For zoom, since it can be very small, exponential format is better
            // ImGui natively doesn't have a great InputDouble with scientific notation out of the box for arbitrary small values easily in standard C-bindings, 
            // but we can use format string
            igInputDouble("Target Zoom", &state->video_target_zoom, 0.0, 0.0, "%.3e", ImGuiInputTextFlags_None);

            igSeparator();

            igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.2f, 0.7f, 0.2f, 1.0f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0.3f, 0.8f, 0.3f, 1.0f});
            if (igButton("Start Video Render", (ImVec2){-1, 40})) {
                // Configure Tour
                state->m_tour.target_re = state->video_target_re;
                state->m_tour.target_im = state->video_target_im;
                state->m_tour.deep_zoom = state->video_target_zoom;
                
                // Duration is distance / speed. We set speed such that it takes exactly duration_sec
                // But tour logic uses zoom distance. Actually, tour logic in tour.c has a fixed speed or uses time.
                // We'll just set tour to active and we might need to adjust tour.c for fixed duration.
                // For now, we just rely on start_video_recording
                
                state->video_is_rendering = 1;
                state->m_tour.phase = 1; // TOUR_PANNING
                state->m_tour.phase_start = now;
                
                // tour_speed = log(target_zoom/zoom) / duration.
                // It's handled inside app_state_update_tours with a default speed if we don't change it.
                // For a proper video renderer, we will just start the recording here.
                if (start_video_recording(state->video_res_w, state->video_res_h, state->video_fps, 0)) {
                    app_state_push_notification(state, "Video Render Started", now);
                }
            }
            igPopStyleColor(2);
        }

        igEnd();
    }

    // Render active notifications
    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            float elapsed = (now - state->notifications[i].start_time) / 1000.0f;
            if (elapsed > 3.0f) {
                state->notifications[i].active = 0;
            } else {
                char window_name[32];
                snprintf(window_name, sizeof(window_name), "Notification##%d", i);
                igSetNextWindowPos((ImVec2){20.0f, 20.0f + i * 40.0f}, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
                igSetNextWindowBgAlpha(0.7f);
                if (igBegin(window_name, NULL, flags)) {
                    igText("%s", state->notifications[i].message);
                    igEnd();
                }
            }
        }
    }

    if (custom_font) {
        igPopFont();
    }
}
