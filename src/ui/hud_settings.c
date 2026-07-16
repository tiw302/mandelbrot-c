/* hud_settings.c
 *
 * [UI] settings window rendering.
 * [ARCH] extracted from hud_sokol.c to resolve god-object anti-pattern and
 * improve modularity. handles core engine configurations like iterations,
 * threads, and precision modes.
 */
#include "hud_settings.h"
#include "color.h"
#include <stdio.h>

void hud_render_settings_window(AppCommonState* state, int win_w, int win_h, int gpu_mode, int* high_precision_mode, int* use_perturbation) {
    if (!state->show_settings) return;

    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav;

    float window_height = 450.0f;
    if (window_height > win_h * 0.9f) {
        window_height = win_h * 0.9f;
    }
    igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always, (ImVec2){0.5f, 0.5f});
    igSetNextWindowSize((ImVec2){400, window_height}, ImGuiCond_Always);
    igSetNextWindowBgAlpha(0.9f);
    if (igBegin("Engine Settings", NULL, settings_flags)) {
        igTextColored((ImVec4){1.0f, 0.2f, 0.2f, 1.0f}, "[ Engine Settings ]");
        igSeparator();
        
        // max iterations
        int max_iter = state->max_iterations;
        if (igSliderInt("Max Iterations", &max_iter, 100, 100000, "%d", ImGuiSliderFlags_Logarithmic)) {
            state->max_iterations = max_iter;
            state->needs_redraw = 1;
        }
        
        // cpu threads
        if (!gpu_mode) {
            int threads = state->thread_count;
            if (igSliderInt("CPU Threads", &threads, 1, 64, "%d", ImGuiSliderFlags_None)) {
                state->thread_count = threads;
                state->needs_redraw = 1;
            }
        }
        
        // palettes
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

        // fractal types
        const char* fractal_names[] = {
            "Mandelbrot", "Burning Ship", "Tricorn", "Celtic", "Buffalo"
        };
        int current_frac = state->base_fractal;
        if (igCombo_Str_arr("Fractal Type", &current_frac, fractal_names, 5, -1)) {
            state->base_fractal = current_frac;
            state->needs_redraw = 1;
        }

        // precision checkboxes
        bool hp = *high_precision_mode;
        if (igCheckbox("High Precision Mode", &hp)) {
            *high_precision_mode = hp;
            state->needs_redraw = 1;
        }
        
        if (gpu_mode && use_perturbation) {
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
