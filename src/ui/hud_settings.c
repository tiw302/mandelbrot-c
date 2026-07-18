/* hud_settings.c
 *
 * settings window: iterations, threads, precision, palettes, and bookmarks.
 * extracted from hud_sokol.c to keep that file focused.
 */

#include "hud_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "color.h"
#include "sokol_app.h"

void hud_render_settings_window(AppCommonState* state, int win_w, int win_h, int gpu_mode,
                                int* high_precision_mode, int* use_perturbation) {
    if (!state->show_settings) return;

    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    float window_height = 450.0f;
    if (window_height > win_h * 0.9f) {
        window_height = win_h * 0.9f;
    }
    igSetNextWindowPos((ImVec2){win_w / 2.0f, win_h / 2.0f}, ImGuiCond_Always,
                       (ImVec2){0.5f, 0.5f});
    igSetNextWindowSize((ImVec2){400, window_height}, ImGuiCond_Always);
    igSetNextWindowBgAlpha(0.9f);
    if (igBegin("Engine Settings", NULL, settings_flags)) {
        igPushItemWidth(200.0f);
        igTextColored((ImVec4){0.3f, 0.8f, 1.0f, 1.0f}, "[ Navigation & Position ]");
        igSeparator();
        igText("Camera Position (Press Enter to apply)");
        double re_val = (double)state->cam.view.center_re;
        if (igInputDouble("Real (X)", &re_val, 0.0, 0.0, "%.15g",
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
            state->cam.view.center_re = (precise_float)re_val;
            state->needs_redraw = 1;
        }

        double im_val = (double)state->cam.view.center_im;
        if (igInputDouble("Imaginary (Y)", &im_val, 0.0, 0.0, "%.15g",
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
            state->cam.view.center_im = (precise_float)im_val;
            state->needs_redraw = 1;
        }

        double zoom_val = (double)state->cam.view.zoom;
        if (igInputDouble("Zoom", &zoom_val, 0.0, 0.0, "%.15g",
                          ImGuiInputTextFlags_EnterReturnsTrue)) {
            state->cam.view.zoom = (precise_float)zoom_val;
            state->needs_redraw = 1;
        }

        if (state->julia_mode) {
            igSeparator();
            igText("Julia Constant (C)");
            double j_re = state->julia_c.re;
            if (igInputDouble("C Real", &j_re, 0.0, 0.0, "%.15g",
                              ImGuiInputTextFlags_EnterReturnsTrue)) {
                state->julia_c.re = j_re;
                state->needs_redraw = 1;
            }
            double j_im = state->julia_c.im;
            if (igInputDouble("C Imaginary", &j_im, 0.0, 0.0, "%.15g",
                              ImGuiInputTextFlags_EnterReturnsTrue)) {
                state->julia_c.im = j_im;
                state->needs_redraw = 1;
            }
        }

        igSeparator();
        igTextColored((ImVec4){0.3f, 0.8f, 1.0f, 1.0f}, "[ Render Settings ]");
        igSeparator();
        int max_iter = state->max_iterations;
        if (igSliderInt("Max Iterations", &max_iter, 100, 100000, "%d",
                        ImGuiSliderFlags_Logarithmic)) {
            state->max_iterations = max_iter;
            state->needs_redraw = 1;
        }

        if (!gpu_mode) {
            int threads = state->thread_count;
            if (igSliderInt("CPU Threads", &threads, 1, 64, "%d", ImGuiSliderFlags_None)) {
                state->thread_count = threads;
                state->needs_redraw = 1;
            }
        }

        igSeparator();
        igTextColored((ImVec4){0.3f, 0.8f, 1.0f, 1.0f}, "[ Visuals & Theme ]");
        igSeparator();
        const char* palette_names[32];
        int pal_count = get_palette_count();
        if (pal_count > 32) pal_count = 32;
        for (int i = 0; i < pal_count; i++) {
            palette_names[i] = get_palette_name(i);
        }

        int current_pal = state->palette_idx;
        if (igCombo_Str_arr("Fractal Theme (Color)", &current_pal, palette_names, pal_count, -1)) {
            state->palette_idx = current_pal;
            init_color_palette(state->max_iterations, current_pal);
            state->needs_redraw = 1;
        }

        const char* fractal_names[] = {"Mandelbrot", "Burning Ship", "Tricorn", "Celtic",
                                       "Buffalo"};
        int current_frac = state->base_fractal;
        if (igCombo_Str_arr("Fractal Type", &current_frac, fractal_names, 5, -1)) {
            state->base_fractal = current_frac;
            state->needs_redraw = 1;
        }

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
        igTextColored((ImVec4){0.3f, 0.8f, 1.0f, 1.0f}, "[ Interaction & Controls ]");
        igSeparator();
        float tour_speed = (float)state->tour_speed_multiplier;
        if (igSliderFloat("Tour Speed", &tour_speed, 0.1f, 5.0f, "%.2fx", ImGuiSliderFlags_None)) {
            state->tour_speed_multiplier = (double)tour_speed;
        }

        float zoom_sens = (float)state->zoom_sensitivity;
        if (igSliderFloat("Zoom Sensitivity", &zoom_sens, 0.1f, 3.0f, "%.2fx",
                          ImGuiSliderFlags_None)) {
            state->zoom_sensitivity = (double)zoom_sens;
        }

        if (igButton("Reset View (R)", (ImVec2){-1, 0})) {
            camera_reset(&state->cam);
            state->needs_redraw = 1;
        }
        igSeparator();
        igTextColored((ImVec4){0.3f, 0.8f, 1.0f, 1.0f}, "[ Tools ]");
        igSeparator();
        if (igButton("Open Video Studio", (ImVec2){-1, 0})) {
#ifdef __linux__
            if (access("./build_deep/mandelbrot_video", X_OK) != -1) {
                system("./build_deep/mandelbrot_video &");
            } else if (access("./build_cpu/mandelbrot_video", X_OK) != -1) {
                system("./build_cpu/mandelbrot_video &");
            } else if (access("./mandelbrot_video", X_OK) != -1) {
                system("./mandelbrot_video &");
            } else {
                fprintf(stderr, "mandelbrot_video executable not found\n");
            }
#endif
        }

        bool is_fullscreen = sapp_is_fullscreen();
        if (igCheckbox("Fullscreen", &is_fullscreen)) {
            sapp_toggle_fullscreen();
        }
        igSeparator();
        igTextColored((ImVec4){0.3f, 0.8f, 1.0f, 1.0f}, "[ Bookmarks & Logs ]");
        igSeparator();
        static char new_bookmark_name[64] = "";
        igSetNextItemWidth(150.0f);
        igInputTextWithHint("##NewBookmarkName", "Bookmark name...", new_bookmark_name,
                            sizeof(new_bookmark_name), ImGuiInputTextFlags_None, NULL, NULL);
        igSameLine(0, -1);
        if (igButton("Save Bookmark", (ImVec2){-1, 0})) {
            app_state_save_bookmark_with_name(
                state, new_bookmark_name[0] != '\0' ? new_bookmark_name : NULL);
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
                                  bookmarks[b].center_re, bookmarks[b].center_im,
                                  bookmarks[b].zoom);

                    igPopID();
                }
            }
            igEndChild();
        } else {
            igTextColored((ImVec4){0.6f, 0.6f, 0.6f, 1.0f}, "No bookmarks saved yet.");
        }

        igSeparator();
        igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "Press TAB to close");
        igPopItemWidth();
    }
    igEnd();
}
