/* hud_video_studio.c
 *
 * video export studio ui: ffmpeg settings, resolution presets, and progress tracking.
 * extracted from hud_sokol.c to keep that file focused.
 */

#include "hud_video_studio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "color.h"
#include "screenshot.h"

void hud_render_video_studio(struct ImFont* custom_font, AppCommonState* state, int win_w,
                             int win_h, int* cpu_precision_128, uint32_t now) {
    if (custom_font) {
        igPushFont(custom_font, 0.0f);
    }

    ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    igSetNextWindowPos((ImVec2){0.0f, 0.0f}, ImGuiCond_Always, (ImVec2){0.0f, 0.0f});
    igSetNextWindowSize((ImVec2){(float)win_w, (float)win_h}, ImGuiCond_Always);
    igSetNextWindowBgAlpha(1.0f);

    if (igBegin("Video Export Studio", NULL, settings_flags)) {
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
            (void)snprintf(progress_text, sizeof(progress_text), "%.1f%%", progress * 100.0f);
            igProgressBar(progress, (ImVec2){-1, 0.0f}, progress_text);

            int total_frames = state->video_settings.fps * state->video_settings.duration_sec;
            int frames_rendered = (int)(progress * total_frames);

            if (progress > 0.0f) {
                uint32_t elapsed = now - render_start_time;
                uint32_t eta = (uint32_t)(((float)elapsed / progress) - elapsed);
                igText("Frames: %d / %d  |  ETA: %d seconds", frames_rendered, total_frames,
                       eta / 1000);
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
                    const char* fractal_names[] = {"Mandelbrot", "Burning Ship", "Tricorn",
                                                   "Celtic", "Buffalo"};
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

                    if (state->julia_mode) {
                        igIndent(16.0f);
                        double re = state->julia_c.re;
                        double im = state->julia_c.im;
                        igSetNextItemWidth(150.0f);
                        if (igInputDouble("Real (Re)", &re, 0.0, 0.0, "%.15f",
                                          ImGuiInputTextFlags_None)) {
                            state->julia_c.re = re;
                            state->needs_redraw = 1;
                        }
                        igSetNextItemWidth(150.0f);
                        if (igInputDouble("Imag (Im)", &im, 0.0, 0.0, "%.15f",
                                          ImGuiInputTextFlags_None)) {
                            state->julia_c.im = im;
                            state->needs_redraw = 1;
                        }
                        igUnindent(16.0f);
                    }

                    int max_iter = state->max_iterations;

                    if (igSliderInt("Max Iterations", &max_iter, 100, 100000, "%d",
                                    ImGuiSliderFlags_Logarithmic)) {
                        state->max_iterations = max_iter;
                        state->needs_redraw = 1;
                    }

                    float b_radius = (float)state->bailout_radius;
                    if (igSliderFloat("Bailout Radius", &b_radius, 2.0f, 500.0f, "%.1f",
                                      ImGuiSliderFlags_Logarithmic)) {
                        state->bailout_radius = b_radius;
                        state->needs_redraw = 1;
                    }

                    int threads = state->thread_count;

                    if (igSliderInt("CPU Threads", &threads, 1, 64, "%d", ImGuiSliderFlags_None)) {
                        state->thread_count = threads;
                        state->needs_redraw = 1;
                    }

                    int tile_sizes[] = {8, 16, 32, 64, 128};
                    int current_tile = state->render_tile_size;
                    if (current_tile == 0) current_tile = 32;
                    const char* tile_names[] = {"8x8", "16x16", "32x32", "64x64", "128x128"};
                    int current_tile_idx = 2;
                    for (int i = 0; i < 5; i++)
                        if (tile_sizes[i] == current_tile) current_tile_idx = i;
                    if (igCombo_Str_arr("Render Tile Size", &current_tile_idx, tile_names, 5, -1)) {
                        state->render_tile_size = tile_sizes[current_tile_idx];
                        state->needs_redraw = 1;
                    }

                    const char* res_names[] = {"100% (1x)", "50% (2x2)", "25% (4x4)",
                                               "12.5% (8x8)"};
                    int res_scales[] = {1, 2, 4, 8};
                    int current_res = state->resolution_scale;
                    if (current_res == 0) current_res = 1;
                    int current_res_idx = 0;
                    for (int i = 0; i < 4; i++)
                        if (res_scales[i] == current_res) current_res_idx = i;
                    if (igCombo_Str_arr("Downscale (Blocky)", &current_res_idx, res_names, 4, -1)) {
                        state->resolution_scale = res_scales[current_res_idx];
                        state->needs_redraw = 1;
                    }

                    const char* palette_names[32];
                    int pal_count = get_palette_count();
                    if (pal_count > 32) pal_count = 32;
                    for (int i = 0; i < pal_count; i++) {
                        palette_names[i] = get_palette_name(i);
                    }

                    int current_pal = state->palette_idx;

                    if (igCombo_Str_arr("Color Palette", &current_pal, palette_names, pal_count,
                                        -1)) {
                        state->palette_idx = current_pal;
                        init_color_palette(state->max_iterations, current_pal);
                        state->needs_redraw = 1;
                    }

                    float c_offset = (float)state->color_offset;
                    if (igSliderFloat("Color Offset", &c_offset, 0.0f, 1.0f, "%.2f",
                                      ImGuiSliderFlags_None)) {
                        state->color_offset = c_offset;
                        set_color_tuning(state->color_offset, state->color_density);
                        init_color_palette(state->max_iterations, state->palette_idx);
                        state->needs_redraw = 1;
                    }
                    float c_density = (float)state->color_density;
                    if (igSliderFloat("Color Density", &c_density, 0.1f, 10.0f, "%.2f",
                                      ImGuiSliderFlags_None)) {
                        state->color_density = c_density;
                        set_color_tuning(state->color_offset, state->color_density);
                        init_color_palette(state->max_iterations, state->palette_idx);
                        state->needs_redraw = 1;
                    }

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
                        igBeginChild_Str("BookmarksList", (ImVec2){0, -60}, true,
                                         ImGuiWindowFlags_None);
                        static int selected_bookmark = -1;
                        for (int b = 0; b < count; b++) {
                            igPushID_Int(b);

                            if (igButton("X", (ImVec2){0, 0})) {
                                app_state_delete_bookmark(state, b);
                                if (selected_bookmark == b) selected_bookmark = -1;
                                igPopID();
                                continue;  // skip rendering the rest of this row
                            }
                            igSameLine(0, -1);
                            bool is_selected = (selected_bookmark == b);
                            if (is_selected) {
                                igPushStyleColor_Vec4(ImGuiCol_Header,
                                                      (ImVec4){0.2f, 0.6f, 1.0f, 0.5f});
                                igPushStyleColor_Vec4(ImGuiCol_HeaderHovered,
                                                      (ImVec4){0.3f, 0.7f, 1.0f, 0.6f});
                            }

                            char label[128];
                            snprintf(label, sizeof(label), "%s %s", is_selected ? ">>" : "  ",
                                     bookmarks[b].name);

                            if (igSelectable_Bool(label, is_selected, ImGuiSelectableFlags_None,
                                                  (ImVec2){0, 0})) {
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

                    igText("Path Type:");
                    int current_path_type = state->video_settings.path_type;
                    if (igRadioButton_IntPtr("Scenic Tour (Presets)", &current_path_type, 0)) {
                        state->video_settings.path_type = 0;
                    }
                    igSameLine(0, 10.0f);
                    if (igRadioButton_IntPtr("Bookmarks Tour (Dynamic)", &current_path_type, 1)) {
                        state->video_settings.path_type = 1;
                    }
                    igSameLine(0, 10.0f);
                    if (igRadioButton_IntPtr("Custom Target", &current_path_type, 2)) {
                        state->video_settings.path_type = 2;
                    }
                    igSpacing();

                    /* ASPECT RATIO & RESOLUTION SELECTION
                     * this section provides a hierarchical menu to select the video
                     * resolution based on predefined aspect ratios (e.g. 16:9, 4:3, etc.)
                     * or a "Custom" resolution. The logic maps the current width/height
                     * to an aspect ratio preset if it exactly matches the proportions. */

                    const char* ar_names[] = {"16:9 (Widescreen)",
                                              "4:3 (Classic Monitor/iPad)",
                                              "16:10 (MacBook/PC)",
                                              "21:9 (Ultrawide)",
                                              "32:9 (Super Ultrawide)",
                                              "9:16 (Story/TikTok)",
                                              "3:4 (Vertical Photo)",
                                              "4:5 (Insta Vertical)",
                                              "1:1 (Square)",
                                              "3:2 (Photography/Surface)",
                                              "2:3 (Vertical 3:2)",
                                              "5:4 (Early LCD)",
                                              "Custom"};
                    static int current_ar_index = 0;
                    int res_w = state->video_settings.res_w;
                    int res_h = state->video_settings.res_h;

                    if (res_w * 9 == res_h * 16)
                        current_ar_index = 0;
                    else if (res_w * 3 == res_h * 4)
                        current_ar_index = 1;
                    else if (res_w * 10 == res_h * 16)
                        current_ar_index = 2;
                    else if (res_w * 9 == res_h * 21)
                        current_ar_index = 3;
                    else if (res_w * 9 == res_h * 32)
                        current_ar_index = 4;
                    else if (res_w * 16 == res_h * 9)
                        current_ar_index = 5;
                    else if (res_w * 4 == res_h * 3)
                        current_ar_index = 6;
                    else if (res_w * 5 == res_h * 4)
                        current_ar_index = 7;
                    else if (res_w == res_h)
                        current_ar_index = 8;
                    else if (res_w * 2 == res_h * 3)
                        current_ar_index = 9;
                    else if (res_w * 3 == res_h * 2)
                        current_ar_index = 10;
                    else if (res_w * 4 == res_h * 5)
                        current_ar_index = 11;
                    else
                        current_ar_index = 12;

                    if (igCombo_Str_arr("Aspect Ratio", &current_ar_index, ar_names, 13, -1)) {
                        if (current_ar_index == 0) {
                            state->video_settings.res_w = 1920;
                            state->video_settings.res_h = 1080;
                        } else if (current_ar_index == 1) {
                            state->video_settings.res_w = 1024;
                            state->video_settings.res_h = 768;
                        } else if (current_ar_index == 2) {
                            state->video_settings.res_w = 1920;
                            state->video_settings.res_h = 1200;
                        } else if (current_ar_index == 3) {
                            state->video_settings.res_w = 2560;
                            state->video_settings.res_h = 1080;
                        } else if (current_ar_index == 4) {
                            state->video_settings.res_w = 5120;
                            state->video_settings.res_h = 1440;
                        } else if (current_ar_index == 5) {
                            state->video_settings.res_w = 1080;
                            state->video_settings.res_h = 1920;
                        } else if (current_ar_index == 6) {
                            state->video_settings.res_w = 1080;
                            state->video_settings.res_h = 1440;
                        } else if (current_ar_index == 7) {
                            state->video_settings.res_w = 1080;
                            state->video_settings.res_h = 1350;
                        } else if (current_ar_index == 8) {
                            state->video_settings.res_w = 1080;
                            state->video_settings.res_h = 1080;
                        } else if (current_ar_index == 9) {
                            state->video_settings.res_w = 2160;
                            state->video_settings.res_h = 1440;
                        } else if (current_ar_index == 10) {
                            state->video_settings.res_w = 1440;
                            state->video_settings.res_h = 2160;
                        } else if (current_ar_index == 11) {
                            state->video_settings.res_w = 1280;
                            state->video_settings.res_h = 1024;
                        }
                    }

                    if (current_ar_index != 12) {
                        // if an aspect ratio preset is selected (not "Custom"), show a sub-menu
                        // of common standard resolutions for that specific aspect ratio.
                        typedef struct {
                            int w;
                            int h;
                            const char* name;
                        } ResPreset;
                        static const ResPreset pre_16_9[] = {
                            {1280, 720, "1280x720 (720p)"}, {1920, 1080, "1920x1080 (1080p)"},
                            {2560, 1440, "2560x1440 (2K)"}, {3840, 2160, "3840x2160 (4K)"},
                            {5120, 2880, "5120x2880 (5K)"}, {7680, 4320, "7680x4320 (8K)"}};
                        static const ResPreset pre_4_3[] = {
                            {640, 480, "640x480 (VGA)"},      {800, 600, "800x600 (SVGA)"},
                            {1024, 768, "1024x768 (XGA)"},    {1440, 1080, "1440x1080 (HD)"},
                            {1600, 1200, "1600x1200 (UXGA)"}, {2048, 1536, "2048x1536 (iPad)"}};
                        static const ResPreset pre_16_10[] = {{1280, 800, "1280x800 (WXGA)"},
                                                              {1440, 900, "1440x900 (WSXGA)"},
                                                              {1680, 1050, "1680x1050 (WSXGA+)"},
                                                              {1920, 1200, "1920x1200 (WUXGA)"},
                                                              {2560, 1600, "2560x1600 (WQXGA)"}};
                        static const ResPreset pre_21_9[] = {{2560, 1080, "2560x1080 (UW FHD)"},
                                                             {3440, 1440, "3440x1440 (UW QHD)"},
                                                             {5120, 2160, "5120x2160 (UW 5K)"}};
                        static const ResPreset pre_32_9[] = {{3840, 1080, "3840x1080 (DFHD)"},
                                                             {5120, 1440, "5120x1440 (DQHD)"}};
                        static const ResPreset pre_9_16[] = {{720, 1280, "720x1280 (Vert HD)"},
                                                             {1080, 1920, "1080x1920 (Vert FHD)"},
                                                             {1440, 2560, "1440x2560 (Vert QHD)"},
                                                             {2160, 3840, "2160x3840 (Vert 4K)"}};
                        static const ResPreset pre_3_4[] = {{480, 640, "480x640 (Vert VGA)"},
                                                            {768, 1024, "768x1024 (Vert XGA)"},
                                                            {1080, 1440, "1080x1440 (Vert HD)"},
                                                            {1536, 2048, "1536x2048 (iPad Vert)"}};
                        static const ResPreset pre_4_5[] = {
                            {1080, 1350, "1080x1350 (Insta Standard)"}, {1440, 1800, "1440x1800"}};
                        static const ResPreset pre_1_1[] = {{1080, 1080, "1080x1080 (Square)"},
                                                            {1440, 1440, "1440x1440 (Square)"},
                                                            {2160, 2160, "2160x2160 (Square)"}};
                        static const ResPreset pre_3_2[] = {{720, 480, "720x480 (DVD)"},
                                                            {1440, 960, "1440x960"},
                                                            {2160, 1440, "2160x1440 (Surface)"},
                                                            {3000, 2000, "3000x2000"}};
                        static const ResPreset pre_2_3[] = {
                            {480, 720, "480x720 (Vert DVD)"},
                            {960, 1440, "960x1440"},
                            {1440, 2160, "1440x2160 (Vert Surface)"}};
                        static const ResPreset pre_5_4[] = {{1280, 1024, "1280x1024 (SXGA)"},
                                                            {2560, 2048, "2560x2048 (QSXGA)"}};

                        const ResPreset* current_presets = NULL;
                        int num_presets = 0;
                        if (current_ar_index == 0) {
                            current_presets = pre_16_9;
                            num_presets = 6;
                        } else if (current_ar_index == 1) {
                            current_presets = pre_4_3;
                            num_presets = 6;
                        } else if (current_ar_index == 2) {
                            current_presets = pre_16_10;
                            num_presets = 5;
                        } else if (current_ar_index == 3) {
                            current_presets = pre_21_9;
                            num_presets = 3;
                        } else if (current_ar_index == 4) {
                            current_presets = pre_32_9;
                            num_presets = 2;
                        } else if (current_ar_index == 5) {
                            current_presets = pre_9_16;
                            num_presets = 4;
                        } else if (current_ar_index == 6) {
                            current_presets = pre_3_4;
                            num_presets = 4;
                        } else if (current_ar_index == 7) {
                            current_presets = pre_4_5;
                            num_presets = 2;
                        } else if (current_ar_index == 8) {
                            current_presets = pre_1_1;
                            num_presets = 3;
                        } else if (current_ar_index == 9) {
                            current_presets = pre_3_2;
                            num_presets = 4;
                        } else if (current_ar_index == 10) {
                            current_presets = pre_2_3;
                            num_presets = 3;
                        } else if (current_ar_index == 11) {
                            current_presets = pre_5_4;
                            num_presets = 2;
                        }

                        const char* preset_names[12];
                        int selected_preset = -1;
                        for (int i = 0; i < num_presets; i++) {
                            preset_names[i] = current_presets[i].name;
                            if (state->video_settings.res_w == current_presets[i].w &&
                                state->video_settings.res_h == current_presets[i].h) {
                                selected_preset = i;
                            }
                        }

                        bool custom_size_for_ar = (selected_preset == -1);
                        if (custom_size_for_ar) {
                            preset_names[num_presets] = "Custom Size";
                            selected_preset = num_presets;
                            num_presets++;
                        }

                        if (igCombo_Str_arr("Resolution Preset", &selected_preset, preset_names,
                                            num_presets, -1)) {
                            if (!custom_size_for_ar || selected_preset != num_presets - 1) {
                                state->video_settings.res_w = current_presets[selected_preset].w;
                                state->video_settings.res_h = current_presets[selected_preset].h;
                            }
                        }
                    }

                    /* always allow manual override of Output Resolution via standard inputs.
                     * this allows users to type in any resolution, even if they selected an aspect
                     * ratio. */
                    igText("Output Resolution:");
                    igSetNextItemWidth(100.0f);
                    igInputInt("Width", &state->video_settings.res_w, 1, 100,
                               ImGuiInputTextFlags_None);
                    igSameLine(0, -1);
                    igSetNextItemWidth(100.0f);
                    igInputInt("Height", &state->video_settings.res_h, 1, 100,
                               ImGuiInputTextFlags_None);

                    const char* aa_names[] = {"None (1x)", "Super Sampling (2x)",
                                              "Super Sampling (4x)"};

                    igCombo_Str_arr("Anti-Aliasing", &state->video_settings.aa_level, aa_names, 3,
                                    -1);

                    /* ADVANCED VIDEO & ENCODING SETTINGS
                     * these fields configure exactly how FFmpeg will mux and encode
                     * the output. They map to various FFmpeg CLI flags in screenshot.c. */

                    igSliderFloat("Color Animation Speed", &state->video_settings.color_cycle_speed,
                                  -10.0f, 10.0f, "%.2f", ImGuiSliderFlags_None);
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igSetTooltip(
                            "Changes the color offset dynamically over the video duration.");
                    }

                    igInputText("Audio Track (.mp3/.wav)", state->video_settings.audio_track_path,
                                sizeof(state->video_settings.audio_track_path),
                                ImGuiInputTextFlags_None, NULL, NULL);
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igSetTooltip(
                            "Optional: Absolute or relative path to an audio file to mux into the "
                            "video.");
                    }

                    const char* pix_names[] = {"YUV420p (Standard)", "YUV444p (High Quality)",
                                               "RGB24 (Lossless Color)"};
                    igCombo_Str_arr("Pixel Format", &state->video_settings.pixel_format_idx,
                                    pix_names, 3, -1);

                    const char* bit_modes[] = {"Quality (CRF)", "Target Bitrate (kbps)"};
                    igCombo_Str_arr("Bitrate Mode", &state->video_settings.bitrate_mode, bit_modes,
                                    2, -1);

                    /* show different input controls depending on whether the user wants to use
                     * CRF (Constant Rate Factor, dynamically allocates bitrate based on complexity)
                     * or a static Target Bitrate (good for strict file size limits). */
                    if (state->video_settings.bitrate_mode == 0) {
                        igSliderInt("Quality (CRF)", &state->video_settings.crf_val, 0, 51, "%d",
                                    ImGuiSliderFlags_None);
                        if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                            igSetTooltip(
                                "CRF (0-51): 0 is lossless, 18 is high, 23 is medium, 28 is low "
                                "quality.");
                        }
                    } else {
                        igInputInt("Target Bitrate (kbps)",
                                   &state->video_settings.target_bitrate_kbps, 500, 5000,
                                   ImGuiInputTextFlags_None);
                    }

                    /* motion blur is a complex feature that uses sub-frame accumulation.
                     * The renderer will generate (samples) number of frames for every actual video
                     * frame, blending them together for cinematic smooth movement, but it increases
                     * render time. */
                    igSliderInt("Motion Blur Samples", &state->video_settings.motion_blur_samples,
                                1, 16, "%d", ImGuiSliderFlags_None);
                    if (igIsItemHovered(ImGuiHoveredFlags_None)) {
                        igSetTooltip(
                            "Multi-samples each frame to simulate shutter blur. (1 = off, higher = "
                            "smoother but slower render)");
                    }

                    const char* preset_names[] = {"ultrafast", "superfast", "veryfast",
                                                  "faster",    "fast",      "medium",
                                                  "slow",      "slower",    "veryslow"};

                    igCombo_Str_arr("Encoding Speed", &state->video_settings.preset_idx,
                                    preset_names, 9, -1);

                    const char* codec_names[] = {"H.264 (libx264)", "H.265 (libx265)"};

                    igCombo_Str_arr("Video Codec", &state->video_settings.codec_idx, codec_names, 2,
                                    -1);

                    bool show_log = state->video_settings.show_log;
                    if (igCheckbox("Show Log on Video", &show_log)) {
                        state->video_settings.show_log = show_log;
                    }
                    if (state->video_settings.show_log) {
                        igIndent(10.0f);

                        igInputInt("Log Size", &state->video_settings.log_fontsize, 1, 5,
                                   ImGuiInputTextFlags_None);

                        igInputText("Log Font Path", state->video_settings.log_fontpath,
                                    sizeof(state->video_settings.log_fontpath),
                                    ImGuiInputTextFlags_None, NULL, NULL);

                        const char* pos_names[] = {"Bottom Left (scroll up)",
                                                   "Bottom Right (scroll up)",
                                                   "Top Left (flow down)", "Top Right (flow down)"};
                        int current_pos = state->video_settings.log_position;

                        if (igCombo_Str_arr("Log Position", &current_pos, pos_names, 4, -1)) {
                            state->video_settings.log_position = current_pos;
                        }

                        const char* color_names[] = {"white", "yellow",  "cyan",  "green",
                                                     "red",   "magenta", "orange"};
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

                        igUnindent(10.0f);
                    }

                    igSliderInt("FPS", &state->video_settings.fps, 24, 120, "%d",
                                ImGuiSliderFlags_None);

                    igSliderInt("Duration (s)", &state->video_settings.duration_sec, 1, 300, "%d",
                                ImGuiSliderFlags_None);

                    const char* curve_names[] = {"Ease In/Out (Smoothstep)",
                                                 "Linear (Constant Speed)", "Ease In (Quadratic)",
                                                 "Ease Out (Quadratic)"};
                    int current_curve = state->video_settings.zoom_curve;

                    if (igCombo_Str_arr("Animation Curve", &current_curve, curve_names, 4, -1)) {
                        state->video_settings.zoom_curve = current_curve;
                    }

                    igText("Output Filename:");

                    igInputText("##OutputFilename", state->video_settings.output_filename,
                                sizeof(state->video_settings.output_filename),
                                ImGuiInputTextFlags_None, NULL, NULL);

                    igBeginDisabled(state->video_settings.path_type != 2);
                    igSeparator();
                    igText("Target Destination:");

                    int count = 0;
                    const Bookmark* bookmarks = app_state_get_bookmarks_array(&count);
                    if (count > 0 && bookmarks) {
                        if (igBeginCombo("Load from Bookmark", "Select...", ImGuiComboFlags_None)) {
                            for (int b = 0; b < count; b++) {
                                igPushID_Int(b);
                                if (igSelectable_Bool(bookmarks[b].name, false,
                                                      ImGuiSelectableFlags_None, (ImVec2){0, 0})) {
                                    snprintf(state->video_settings.target_re,
                                             sizeof(state->video_settings.target_re), "%.15f",
                                             (double)bookmarks[b].center_re);
                                    snprintf(state->video_settings.target_im,
                                             sizeof(state->video_settings.target_im), "%.15f",
                                             (double)bookmarks[b].center_im);
                                    snprintf(state->video_settings.target_zoom,
                                             sizeof(state->video_settings.target_zoom), "%.3e",
                                             (double)bookmarks[b].zoom);
                                }
                                igPopID();
                            }
                            igEndCombo();
                        }
                    } else {
                        igTextColored((ImVec4){0.5f, 0.5f, 0.5f, 1.0f}, "(No bookmarks available)");
                    }

                    igInputText("Target Re", state->video_settings.target_re,
                                sizeof(state->video_settings.target_re), ImGuiInputTextFlags_None,
                                NULL, NULL);

                    igInputText("Target Im", state->video_settings.target_im,
                                sizeof(state->video_settings.target_im), ImGuiInputTextFlags_None,
                                NULL, NULL);

                    igInputText("Target Zoom", state->video_settings.target_zoom,
                                sizeof(state->video_settings.target_zoom), ImGuiInputTextFlags_None,
                                NULL, NULL);

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
                igTextColored((ImVec4){1.0f, 0.6f, 0.2f, 1.0f},
                              "Warning: File '%s' already exists and will be overwritten.",
                              state->video_settings.output_filename);
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

            if (igButton(
                    file_exists && can_render ? "Overwrite Video Render" : "Start Video Render",
                    (ImVec2){-1, 40})) {
                start_video_export_async(state);
                app_state_push_notification(state, "Video Render Started", now);
            }
            igPopStyleColor(2);
            igEndDisabled();
        }

        igEnd();
    }

    for (int i = 0; i < 5; i++) {
        if (state->notifications[i].active) {
            float elapsed = (now - state->notifications[i].start_time) / 1000.0f;
            if (elapsed > 3.0f) {
                state->notifications[i].active = 0;
            } else {
                char window_name[32];
                (void)snprintf(window_name, sizeof(window_name), "Notification##%d", i);
                igSetNextWindowPos((ImVec2){win_w - 20.0f, win_h - 20.0f - i * 40.0f},
                                   ImGuiCond_Always, (ImVec2){1.0f, 1.0f});
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
