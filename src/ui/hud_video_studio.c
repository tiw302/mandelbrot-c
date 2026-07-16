/* hud_video_studio.c
 *
 * [UI] video export studio rendering.
 * [ARCH] extracted from hud_sokol.c to resolve god-object anti-pattern.
 * manages ffmpeg rendering configurations, resolution presets, and progress tracking.
 */
#include "hud_video_studio.h"
#include "color.h"
#include "screenshot.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void hud_render_video_studio(struct ImFont* custom_font, AppCommonState* state,
                             int win_w, int win_h, int* cpu_precision_128, uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }

    ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav;

    float window_height = state->video_settings.is_rendering ? 140.0f : 550.0f;
    if (window_height > win_h * 0.9f) {
        window_height = win_h * 0.9f;
    }
    igSetNextWindowPos((ImVec2){win_w * 0.5f, win_h * 0.5f}, ImGuiCond_Always, (ImVec2){0.5f, 0.5f});
    igSetNextWindowSize((ImVec2){650, window_height}, ImGuiCond_Always);
    igSetNextWindowBgAlpha(0.9f);

    if (igBegin("Video Export Studio", NULL, settings_flags)) {
        igTextColored((ImVec4){0.2f, 0.8f, 1.0f, 1.0f}, "[ Video Export Studio ]");
        igSeparator();

        static int was_rendering = 0;

        if (state->video_settings.is_rendering) {
            static uint32_t render_start_time = 0;
            if (!was_rendering) {
                render_start_time = now;
            }
            was_rendering = 1;

            igText("Rendering in progress...");
            float progress = state->video_settings.export_progress_percent;
            char progress_text[32];
            snprintf(progress_text, sizeof(progress_text), "%.1f%%", progress * 100.0f);
            igProgressBar(progress, (ImVec2){-1, 0.0f}, progress_text);

            int total_frames = state->video_settings.fps * state->video_settings.duration_sec;
            int frames_rendered = (int)(progress * total_frames);
            
            if (progress > 0.0f) {
                uint32_t elapsed = now - render_start_time;
                uint32_t eta = (uint32_t)(((float)elapsed / progress) - elapsed);
                igText("Frames: %d / %d  |  ETA: %d seconds", frames_rendered, total_frames, eta / 1000);
            } else {
                igText("Frames: 0 / %d  |  ETA: calculating...", total_frames);
            }

            igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.8f, 0.1f, 0.1f, 1.0f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){1.0f, 0.2f, 0.2f, 1.0f});
            if (igButton("Cancel Render", (ImVec2){-1, 0})) {
                state->video_settings.export_cancelled = 1;
            }
            igPopStyleColor(2);
        } else {
            was_rendering = 0;

            if (igBeginTabBar("VideoStudioTabs", ImGuiTabBarFlags_None)) {
                if (igBeginTabItem("Fractal", NULL, ImGuiTabItemFlags_None)) {
                    igSpacing();
            // fractal types
            const char* fractal_names[] = {
                "Mandelbrot", "Burning Ship", "Tricorn", "Celtic", "Buffalo"
            };
            int current_frac = state->base_fractal;

            if (igCombo_Str_arr("Fractal Type", &current_frac, fractal_names, 5, -1)) {
                state->base_fractal = current_frac;
                state->needs_redraw = 1;
            }

            bool julia_mode = state->julia_mode;
            if (igCheckbox("Julia Explorer Mode", &julia_mode)) {
                state->julia_mode = julia_mode;
                state->needs_redraw = 1;
            }

            // iterations
            int max_iter = state->max_iterations;

            if (igSliderInt("Max Iterations", &max_iter, 100, 100000, "%d", ImGuiSliderFlags_Logarithmic)) {
                state->max_iterations = max_iter;
                state->needs_redraw = 1;
            }

            // cpu threads
            int threads = state->thread_count;

            if (igSliderInt("CPU Threads", &threads, 1, 64, "%d", ImGuiSliderFlags_None)) {
                state->thread_count = threads;
                state->needs_redraw = 1;
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

            // cpu precision
            if (cpu_precision_128) {
                bool is_128 = *cpu_precision_128;
                if (igCheckbox("128-bit Float", &is_128)) {
                    *cpu_precision_128 = is_128;
                    state->needs_redraw = 1;
                }
            }

                    igEndTabItem();
                }

                if (igBeginTabItem("Bookmarks", NULL, ImGuiTabItemFlags_None)) {
                    igSpacing();

            int count;
            const Bookmark* bookmarks = app_state_get_bookmarks_array(&count);
            if (count > 0 && bookmarks) {
                igText("Saved Bookmarks (%d):", count);
                igBeginChild_Str("BookmarksList", (ImVec2){0, -60}, true, ImGuiWindowFlags_None);
                static int selected_bookmark = -1;
                for (int b = 0; b < count; b++) {
                    igPushID_Int(b);

                    if (igButton("X", (ImVec2){0, 0})) {
                        app_state_delete_bookmark(state, b);
                        if (selected_bookmark == b) selected_bookmark = -1;
                        igPopID();
                        continue; // skip rendering the rest of this row
                    }
                    igSameLine(0, -1);
                    bool is_selected = (selected_bookmark == b);
                    if (is_selected) {
                        igPushStyleColor_Vec4(ImGuiCol_Header, (ImVec4){0.2f, 0.6f, 1.0f, 0.5f});
                        igPushStyleColor_Vec4(ImGuiCol_HeaderHovered, (ImVec4){0.3f, 0.7f, 1.0f, 0.6f});
                    }
                    
                    char label[128];
                    snprintf(label, sizeof(label), "%s %s", is_selected ? ">>" : "  ", bookmarks[b].name);
                    
                    if (igSelectable_Bool(label, is_selected, ImGuiSelectableFlags_None, (ImVec2){0,0})) {
                        selected_bookmark = b;
                        app_state_load_bookmark(state, b);
                    }

                    if (is_selected) {
                        igPopStyleColor(2);
                    }
                    igPopID();
                }
                igEndChild();
            } else {
                igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "(No bookmarks saved)");
            }

                    igEndTabItem();
                }
                
                if (igBeginTabItem("Output", NULL, ImGuiTabItemFlags_None)) {
                    igSpacing();

            // path type selection
            igText("Path Type:");
            int current_path_type = state->video_settings.path_type;
            if (igRadioButton_IntPtr("Scenic Tour (Presets)", &current_path_type, 0)) { state->video_settings.path_type = 0; }
            igSameLine(0, 10.0f);
            if (igRadioButton_IntPtr("Bookmarks Tour (Dynamic)", &current_path_type, 1)) { state->video_settings.path_type = 1; }
            igSameLine(0, 10.0f);
            if (igRadioButton_IntPtr("Custom Target", &current_path_type, 2)) { state->video_settings.path_type = 2; }
            igSpacing();

            typedef struct {
                int w;
                int h;
            } ResPreset;
            static const ResPreset presets[] = {
                {0, 0},         // custom
                {1280, 720},    // 16:9 720p
                {1920, 1080},   // 16:9 1080p
                {2560, 1440},   // 16:9 2k
                {3840, 2160},   // 16:9 4k
                {720, 1280},    // 9:16 vertical hd
                {1080, 1920},   // 9:16 vertical fhd
                {1440, 2560},   // 9:16 vertical qhd
                {640, 480},     // 4:3 vga
                {800, 600},     // 4:3 svga
                {1024, 768},    // 4:3 xga
                {1600, 1200},   // 4:3 uxga
                {1280, 800},    // 16:10 wxga
                {1920, 1200},   // 16:10 wuxga
                {2560, 1600},   // 16:10 wqxga
                {2560, 1080},   // 21:9 uw fhd
                {3440, 1440},   // 21:9 uw qhd
                {1080, 1080}    // 1:1 square
            };
            const char* res_names[] = {
                "Custom",
                "1280x720 (16:9 720p)",
                "1920x1080 (16:9 1080p)",
                "2560x1440 (16:9 2K)",
                "3840x2160 (16:9 4K)",
                "720x1280 (9:16 Vert HD)",
                "1080x1920 (9:16 Vert FHD)",
                "1440x2560 (9:16 Vert QHD)",
                "640x480 (4:3 VGA)",
                "800x600 (4:3 SVGA)",
                "1024x768 (4:3 XGA)",
                "1600x1200 (4:3 UXGA)",
                "1280x800 (16:10 WXGA)",
                "1920x1200 (16:10 WUXGA)",
                "2560x1600 (16:10 WQXGA)",
                "2560x1080 (21:9 UW FHD)",
                "3440x1440 (21:9 UW QHD)",
                "1080x1080 (1:1 Square)"
            };
            int res_count = sizeof(presets) / sizeof(presets[0]);
            int current_preset = 0;
            for (int i = 1; i < res_count; i++) {
                if (state->video_settings.res_w == presets[i].w && state->video_settings.res_h == presets[i].h) {
                    current_preset = i;
                    break;
                }
            }

            if (igCombo_Str_arr("Resolution Preset", &current_preset, res_names, res_count, -1)) {
                if (current_preset > 0) {
                    state->video_settings.res_w = presets[current_preset].w;
                    state->video_settings.res_h = presets[current_preset].h;
                }
            }

            igText("Output Resolution:");
            igSetNextItemWidth(100.0f);
            igInputInt("Width", &state->video_settings.res_w, 1, 100, ImGuiInputTextFlags_None);
            igSameLine(0, -1);
            igSetNextItemWidth(100.0f);
            igInputInt("Height", &state->video_settings.res_h, 1, 100, ImGuiInputTextFlags_None);

            const char* aa_names[] = {"None (1x)", "Super Sampling (2x)", "Super Sampling (4x)"};

            igCombo_Str_arr("Anti-Aliasing", &state->video_settings.aa_level, aa_names, 3, -1);


            igSliderInt("Quality (CRF)", &state->video_settings.crf_val, 0, 51, "%d", ImGuiSliderFlags_None);
            if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                igSetTooltip("CRF (0-51): 0 is lossless, 18 is high, 23 is medium, 28 is low quality.");
            }

            const char* preset_names[] = {"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"};

            igCombo_Str_arr("Encoding Speed", &state->video_settings.preset_idx, preset_names, 9, -1);

            const char* codec_names[] = {"H.264 (libx264)", "H.265 (libx265)"};

            igCombo_Str_arr("Video Codec", &state->video_settings.codec_idx, codec_names, 2, -1);

            bool show_log = state->video_settings.show_log;
            if (igCheckbox("Show Log on Video", &show_log)) {
                state->video_settings.show_log = show_log;
            }
            if (state->video_settings.show_log) {
                igIndent(10.0f);

    
                igInputInt("Log Size", &state->video_settings.log_fontsize, 1, 5, ImGuiInputTextFlags_None);

    
                igInputText("Log Font Path", state->video_settings.log_fontpath, sizeof(state->video_settings.log_fontpath), ImGuiInputTextFlags_None, NULL, NULL);

                const char* pos_names[] = {"Top Left", "Top Right", "Bottom Left", "Bottom Right"};
                int current_pos = state->video_settings.log_position;
    
                if (igCombo_Str_arr("Log Position", &current_pos, pos_names, 4, -1)) {
                    state->video_settings.log_position = current_pos;
                }

                const char* color_names[] = {"white", "yellow", "cyan", "green", "red", "magenta", "orange"};
                int color_idx = 0;
                for (int i = 0; i < 7; i++) {
                    if (strcmp(state->video_settings.log_fontcolor, color_names[i]) == 0) {
                        color_idx = i;
                        break;
                    }
                }
    
                if (igCombo_Str_arr("Log Color", &color_idx, color_names, 7, -1)) {
                    strcpy(state->video_settings.log_fontcolor, color_names[color_idx]);
                }

    
                igSliderFloat("Box Opacity", &state->video_settings.log_opacity, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None);

                igUnindent(10.0f);
            }

            // fps and duration

            igSliderInt("FPS", &state->video_settings.fps, 24, 120, "%d", ImGuiSliderFlags_None);

            igSliderInt("Duration (s)", &state->video_settings.duration_sec, 1, 300, "%d", ImGuiSliderFlags_None);

            // zoom curve selection
            const char* curve_names[] = {"Ease In/Out (Smoothstep)", "Linear (Constant Speed)", "Ease In (Quadratic)", "Ease Out (Quadratic)"};
            int current_curve = state->video_settings.zoom_curve;

            if (igCombo_Str_arr("Animation Curve", &current_curve, curve_names, 4, -1)) {
                state->video_settings.zoom_curve = current_curve;
            }

            // output filename input
            igText("Output Filename:");

            igInputText("##OutputFilename", state->video_settings.output_filename, sizeof(state->video_settings.output_filename), ImGuiInputTextFlags_None, NULL, NULL);

            igBeginDisabled(state->video_settings.path_type != 2);
                igSeparator();
                igText("Target Destination:");

                // load target from bookmark
                int count = 0;
                const Bookmark* bookmarks = app_state_get_bookmarks_array(&count);
                if (count > 0 && bookmarks) {
                    if (igBeginCombo("Load from Bookmark", "Select...", ImGuiComboFlags_None)) {
                        for (int b = 0; b < count; b++) {
                            igPushID_Int(b);
                            if (igSelectable_Bool(bookmarks[b].name, false, ImGuiSelectableFlags_None, (ImVec2){0,0})) {
                                snprintf(state->video_settings.target_re, sizeof(state->video_settings.target_re), "%.15f", (double)bookmarks[b].center_re);
                                snprintf(state->video_settings.target_im, sizeof(state->video_settings.target_im), "%.15f", (double)bookmarks[b].center_im);
                                snprintf(state->video_settings.target_zoom, sizeof(state->video_settings.target_zoom), "%.3e", (double)bookmarks[b].zoom);
                            }
                            igPopID();
                        }
                        igEndCombo();
                    }
                } else {
                    igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "(No bookmarks available)");
                }

    
                igInputText("Target Re", state->video_settings.target_re, sizeof(state->video_settings.target_re), ImGuiInputTextFlags_None, NULL, NULL);
    
                igInputText("Target Im", state->video_settings.target_im, sizeof(state->video_settings.target_im), ImGuiInputTextFlags_None, NULL, NULL);
    
                igInputText("Target Zoom", state->video_settings.target_zoom, sizeof(state->video_settings.target_zoom), ImGuiInputTextFlags_None, NULL, NULL);
                
                igSpacing();
                if (igButton("Preview Target", (ImVec2){-1, 0})) {
                    state->cam.view.center_re = strtod(state->video_settings.target_re, NULL);
                    state->cam.view.center_im = strtod(state->video_settings.target_im, NULL);
                    state->cam.view.zoom = strtod(state->video_settings.target_zoom, NULL);
                    state->needs_redraw = 1;
                }
            igEndDisabled();

                    igEndTabItem();
                }
                igEndTabBar();
            }

            igSeparator();

            bool can_render = true;
            char error_msg[256] = "";

            if (strlen(state->video_settings.output_filename) == 0) {
                can_render = false;
                strcpy(error_msg, "Error: Output filename is empty.");
            } else if (state->video_settings.duration_sec <= 0) {
                can_render = false;
                strcpy(error_msg, "Error: Duration must be > 0.");
            } else if (state->video_settings.path_type == 2) {
                if (strlen(state->video_settings.target_re) == 0 ||
                    strlen(state->video_settings.target_im) == 0 ||
                    strlen(state->video_settings.target_zoom) == 0) {
                    can_render = false;
                    strcpy(error_msg, "Error: Target coordinates are missing.");
                }
            }

            bool file_exists = (access(state->video_settings.output_filename, F_OK) == 0);
            if (can_render && file_exists) {
                igTextColored((ImVec4){1.0f, 0.6f, 0.2f, 1.0f}, "Warning: File '%s' already exists and will be overwritten.", state->video_settings.output_filename);
            } else if (!can_render) {
                igTextColored((ImVec4){1.0f, 0.2f, 0.2f, 1.0f}, "%s", error_msg);
            }

            igBeginDisabled(!can_render);
            
            if (file_exists && can_render) {
                igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.8f, 0.5f, 0.1f, 1.0f});
                igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){1.0f, 0.6f, 0.2f, 1.0f});
            } else {
                igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.2f, 0.7f, 0.2f, 1.0f});
                igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0.3f, 0.8f, 0.3f, 1.0f});
            }
            
            if (igButton(file_exists && can_render ? "Overwrite Video Render" : "Start Video Render", (ImVec2){-1, 40})) {
                start_video_export_async(state);
                app_state_push_notification(state, "Video Render Started", now);
            }
            igPopStyleColor(2);
            igEndDisabled();
        }

        igEnd();
    }

    // render active notifications
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
                if (igBegin(window_name, NULL, overlay_flags)) {
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
