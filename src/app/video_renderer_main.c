/* 
 * [ARCH] platform entry point: video exporter (headless cli)
 * 
 * this is a specialized entry point that bypasses sokol and imgui entirely.
 * instead of displaying pixels to a screen, it drives the simulation loop 
 * as fast as the cpu can compute, dumping raw rgb frames directly into 
 * a pipe connected to ffmpeg.
 * 
 * separating this from the gui executable ensures that background rendering 
 * can run on servers without an active display (x11/wayland).
 */
#include "app_runner.h"
#include "app_state.h"
#include "renderer.h"
#include "screenshot.h"
#include "tour.h"
#include "color.h"
#include "config_loader.h"
#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "headless_egl.h"

#ifdef __linux__
#include <GL/gl.h>
#include "sokol_gfx.h"
#include "shaders.h"
GLAPI void APIENTRY glReadPixels(int x, int y, int width, int height, unsigned int format, unsigned int type, void *pixels);
#endif

#define SAFE_STRCPY(dest, src) do { \
    snprintf((dest), sizeof(dest), "%s", (src)); \
} while(0)

void print_help(void) {
    printf("Mandelbrot Video Studio CLI Options:\n");
    printf("  -h, --help                 Show this help message and exit\n");
    printf("  --headless                 Run video rendering in CLI headless mode (no GUI)\n");
    printf("  -w, --width <w>            Output video width (default: 1280)\n");
    printf("  -g, --height <h>           Output video height (default: 720)\n");
    printf("  -f, --fps <fps>            Framerate (default: 60)\n");
    printf("  -d, --duration <sec>       Duration of video in seconds (default: 10)\n");
    printf("  -o, --out <filename>       Output video filename (default: mandelbrot_video.mp4)\n");
    printf("  -c, --crf <crf>            CRF quality factor 0-51 (default: 18)\n");
    printf("  -p, --path <type>          Path type: 'scenic', 'bookmarks', 'custom' (default: scenic)\n");
    printf("  -s, --preset <preset>      Encoding speed: ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow (default: fast)\n");
    printf("  --codec <h264|h265>        Video codec (default: h264)\n");
    printf("  --aa <1|2|4>               Anti-aliasing level (default: 1)\n");
    printf("  --log                      Enable log telemetry overlay on video\n");
    printf("  --log-size <size>          Log font size (default: 20)\n");
    printf("  --log-font <path>          Path to custom TrueType font file (default: assets/fonts/font.ttf)\n");
    printf("  --log-pos <0-3>            Log position: 0=Top-Left, 1=Top-Right, 2=Bottom-Left, 3=Bottom-Right (default: 0)\n");
    printf("  --log-opacity <opacity>    Log box background opacity 0.0-1.0 (default: 0.6)\n");
    printf("  --log-color <color>        Log text color (white, yellow, cyan, green, red, etc.) (default: white)\n");
    printf("  --gpu                      Use GPU (EGL headless) instead of CPU for rendering (Linux only)\n");
    printf("  --curve <curve>            Animation zoom curve: 0=Ease In/Out, 1=Linear, 2=Ease In, 3=Ease Out (default: 0)\n");
}

void run_headless_video_render(void) {
    AppCommonState state;
    
    // Load config settings.json
    load_config_from_file("settings.json");

    // Initialize state
    app_state_init(&state, g_cli_args.width, g_cli_args.height);

    // Initialize color palette
    init_color_palette(state.max_iterations, state.palette_idx);

    // Initialize CPU renderer
    RendererContext* render_ctx = init_renderer(state.max_iterations, state.palette_idx);
    if (!render_ctx) {
        fprintf(stderr, "error: failed to initialize renderer\n");
        return;
    }
    set_renderer_thread_count(render_ctx, state.thread_count);

    // Set up standard or custom targets using the shared controller
    app_state_start_video_render(&state, 0);

    const char* presets[] = {"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"};
    const char* codecs[] = {"libx264", "libx265"};
    int aas[] = {1, 2, 4};
    int aa_val = aas[state.video_settings.aa_level];
    int render_w = state.video_settings.res_w * aa_val;
    int render_h = state.video_settings.res_h * aa_val;

    int ok = start_video_recording(state.video_settings.res_w, state.video_settings.res_h, state.video_settings.fps, 0,
                                   state.video_settings.crf_val, presets[state.video_settings.preset_idx],
                                   codecs[state.video_settings.codec_idx], aa_val,
                                   state.video_settings.show_log, state.video_settings.log_fontpath, state.video_settings.log_fontsize,
                                   state.video_settings.output_filename, state.video_settings.log_position, state.video_settings.log_opacity,
                                   state.video_settings.log_fontcolor);
    if (!ok) {
        fprintf(stderr, "error: failed to start video recording (is ffmpeg installed?)\n");
        cleanup_renderer(render_ctx);
        cleanup_color_palette();
        return;
    }

    int total_frames = state.video_settings.duration_sec * state.video_settings.fps;
    uint32_t* vbuf = malloc((size_t)render_w * render_h * 4);
    if (!vbuf) {
        fprintf(stderr, "error: failed to allocate memory for video frames\n");
        stop_video_recording();
        cleanup_renderer(render_ctx);
        cleanup_color_palette();
        return;
    }

    printf("Starting headless video render: %s (%dx%d, %d fps, %d frames)\n",
           state.video_settings.output_filename, state.video_settings.res_w, state.video_settings.res_h, state.video_settings.fps, total_frames);

    for (int frame_idx = 0; frame_idx < total_frames; frame_idx++) {
        uint32_t now = frame_idx * (1000 / state.video_settings.fps);
        
        // Update simulation state (tours and camera boundaries)
        app_state_step_simulation(&state, now);
        
        if (state.m_tour.phase == 0 && frame_idx > 10) {
            // tour finished early
            break;
        }

        precise_float rmin, rmax, imin, imax;
        app_state_calculate_boundaries(&state, render_w, render_h, &rmin, &rmax, &imin, &imax);

        RenderJob job = {
            .pixels = vbuf,
            .pitch = render_w * 4,
            .window_width = render_w,
            .window_height = render_h,
            .re_min = rmin,
            .re_max = rmax,
            .im_top = imax,
            .im_bottom = imin,
            .mode = state.julia_mode ? RENDER_JULIA : state.base_fractal,
            .julia_c = state.julia_c,
            .max_iterations = state.max_iterations
        };

        render_fractal_threaded(render_ctx, &job);

        if (state.video_settings.show_log) {
            FILE* log_f = fopen("video_log.txt", "w");
            if (log_f) {
                fprintf(log_f, "Frame: %d / %d\n", frame_idx + 1, total_frames);
                fprintf(log_f, "Center Re: %.15f\n", (double)state.cam.view.center_re);
                fprintf(log_f, "Center Im: %.15f\n", (double)state.cam.view.center_im);
                fprintf(log_f, "Zoom: %.3e\n", (double)state.cam.view.zoom);
                fprintf(log_f, "Iterations: %d\n", state.max_iterations);
                fclose(log_f);
            }
        }

        /* 
         * [WARNING] synchronous blocking call
         * this passes the raw pixel buffer (vbuf) down the pipe directly to ffmpeg.
         * if the system io is slow, or ffmpeg lags behind, this `fwrite()` inside
         * append_video_frame() will block the entire thread.
         * TODO: tech debt - move this to a dedicated background i/o thread using
         * a ring buffer (e.g. moodycamel::ConcurrentQueue in c++) so the renderer 
         * never waits on ffmpeg to finish encoding a frame.
         */
        append_video_frame(vbuf, render_w, render_h);
        
        // check if the pipe was closed prematurely (e.g., ffmpeg crashed or wasn't found in path)
        // if this happens, we must bail out immediately to prevent a hang.
        if (!is_video_recording()) {
            fprintf(stderr, "\nerror: ffmpeg pipe broken unexpectedly. Aborting.\n");
            break;
        }

        if ((frame_idx + 1) % 5 == 0 || frame_idx == total_frames - 1) {
            printf("Progress: %d%% (%d/%d frames rendered)\r", (frame_idx + 1) * 100 / total_frames, frame_idx + 1, total_frames);
            fflush(stdout);
        }
    }

    printf("\nHeadless render complete! Saving video...\n");
    stop_video_recording();
    free(vbuf);
    cleanup_renderer(render_ctx);
    cleanup_color_palette();
    printf("Video saved successfully as: %s\n", state.video_settings.output_filename);
}

#ifdef __linux__
void run_headless_gpu_video_render(void) {
    AppCommonState state;
    load_config_from_file("settings.json");
    app_state_init(&state, g_cli_args.width, g_cli_args.height);

    int aa_val = (state.video_settings.aa_level == 0) ? 1 : (state.video_settings.aa_level == 1 ? 2 : 4);
    int render_w = state.video_settings.res_w * aa_val;
    int render_h = state.video_settings.res_h * aa_val;

    if (!init_headless_egl(render_w, render_h)) {
        fprintf(stderr, "error: EGL context creation failed. Make sure you are on Linux with a supported GPU.\n");
        return;
    }

    sg_setup(&(sg_desc){
        .environment.defaults.color_format = SG_PIXELFORMAT_RGBA8,
        .environment.defaults.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .environment.defaults.sample_count = 1
    });

    sg_shader shd = sg_make_shader(desktop_gpu_shader_desc(sg_query_backend()));
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {.attrs = {[0].format = SG_VERTEXFORMAT_FLOAT2, [1].format = SG_VERTEXFORMAT_FLOAT2}},
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {.pixel_format = SG_PIXELFORMAT_NONE},
    });

    float verts[] = {-1, 1, 0, 0, 1, 1, 1, 0, 1, -1, 1, 1, -1, -1, 0, 1};
    sg_buffer vbuf_sg = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});
    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    sg_buffer ibuf_sg = sg_make_buffer(&(sg_buffer_desc){.usage = {.index_buffer = true, .immutable = true}, .data = SG_RANGE(idx)});
    sg_sampler smp = sg_make_sampler(&(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR, .mag_filter = SG_FILTER_LINEAR});
    
    static uint32_t dummy_pix[1] = {0xFF000000};
    sg_image dummy_img = sg_make_image(&(sg_image_desc){
        .width = 1, .height = 1, .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.mip_levels[0] = SG_RANGE(dummy_pix)
    });
    sg_view dummy_img_view = sg_make_view(&(sg_view_desc){.texture.image = dummy_img});

    app_state_start_video_render(&state, 0);

    const char* presets[] = {"ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow"};
    const char* codecs[] = {"libx264", "libx265"};
    
    int ok = start_video_recording(state.video_settings.res_w, state.video_settings.res_h, state.video_settings.fps, 0,
                                   state.video_settings.crf_val, presets[state.video_settings.preset_idx],
                                   codecs[state.video_settings.codec_idx], aa_val,
                                   state.video_settings.show_log, state.video_settings.log_fontpath, state.video_settings.log_fontsize,
                                   state.video_settings.output_filename, state.video_settings.log_position, state.video_settings.log_opacity,
                                   state.video_settings.log_fontcolor);
    if (!ok) {
        fprintf(stderr, "error: failed to start video recording (is ffmpeg installed?)\n");
        return;
    }

    int total_frames = state.video_settings.duration_sec * state.video_settings.fps;
    uint32_t* pixel_buf = malloc((size_t)render_w * render_h * 4);
    if (!pixel_buf) {
        fprintf(stderr, "error: out of memory\n");
        return;
    }

    printf("Starting Headless GPU render: %s (%dx%d, %d fps, %d frames)\n", state.video_settings.output_filename, state.video_settings.res_w, state.video_settings.res_h, state.video_settings.fps, total_frames);

    sg_pass_action pass_action = {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = {0,0,0,1} }
    };

    sg_swapchain swapchain = {
        .width = render_w, .height = render_h,
        .color_format = SG_PIXELFORMAT_RGBA8, .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .sample_count = 1, .gl.framebuffer = 0
    };

    for (int frame_idx = 0; frame_idx < total_frames; frame_idx++) {
        uint32_t now = frame_idx * (1000 / state.video_settings.fps);
        app_state_step_simulation(&state, now);
        if (state.m_tour.phase == 0 && frame_idx > 10) break;

        sg_begin_pass(&(sg_pass){.action = pass_action, .swapchain = swapchain});
        sg_apply_pipeline(pip);

        params_t_t params = {0};
        params.u_center_hi[0] = (float)state.cam.view.center_re;
        params.u_center_lo[0] = (float)(state.cam.view.center_re - (double)params.u_center_hi[0]);
        params.u_center_hi[1] = (float)state.cam.view.center_im;
        params.u_center_lo[1] = (float)(state.cam.view.center_im - (double)params.u_center_hi[1]);
        params.u_julia_c_hi[0] = (float)state.julia_c.re;
        params.u_julia_c_lo[0] = (float)(state.julia_c.re - (double)params.u_julia_c_hi[0]);
        params.u_julia_c_hi[1] = (float)state.julia_c.im;
        params.u_julia_c_lo[1] = (float)(state.julia_c.im - (double)params.u_julia_c_hi[1]);
        params.u_zoom = (float)state.cam.view.zoom;
        params.u_iters = (float)state.max_iterations;
        params.u_aspect = (float)render_w / render_h;
        params.u_fractal_type = (float)(state.julia_mode ? 1 : state.base_fractal);
        params.u_palette = (float)state.palette_idx;
        params.u_high_precision = 0;
        params.u_use_perturbation = 0;
        params.u_zoom_lo = (float)(state.cam.view.zoom - (double)params.u_zoom);

        sg_apply_uniforms(UB_params_t, &SG_RANGE(params));
        
        sg_bindings bind = {0};
        bind.vertex_buffers[0] = vbuf_sg;
        bind.index_buffer = ibuf_sg;
        bind.samplers[SMP_u_orbit_smp] = smp;
        bind.views[VIEW_u_orbit] = dummy_img_view;
        bind.samplers[SMP_smp] = smp;
        bind.views[VIEW_tex] = dummy_img_view;
        sg_apply_bindings(&bind);
        sg_draw(0, 6, 1);
        
        glReadPixels(0, 0, render_w, render_h, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buf);
        
        sg_end_pass();
        sg_commit();

        append_video_frame(pixel_buf, render_w, render_h);
        
        if (!is_video_recording()) break;
        if ((frame_idx + 1) % 5 == 0 || frame_idx == total_frames - 1) {
            printf("Progress: %d%% (%d/%d frames rendered)\r", (frame_idx + 1) * 100 / total_frames, frame_idx + 1, total_frames);
            fflush(stdout);
        }
    }
    printf("\nGPU Headless render complete! Saving video...\n");
    stop_video_recording();
    free(pixel_buf);
    sg_shutdown();
}
#endif

sapp_desc sokol_main(int argc, char* argv[]) {
    // Set default values in g_cli_args
    g_cli_args.width = 1280;
    g_cli_args.height = 720;
    g_cli_args.fps = 60;
    g_cli_args.duration = 10;
    g_cli_args.crf = 1;
    g_cli_args.curve = 0;
    g_cli_args.log = 0;
    g_cli_args.log_size = 20;
    g_cli_args.log_pos = 0;
    g_cli_args.log_opacity = 0.6f;
    g_cli_args.aa = 1;
    SAFE_STRCPY(g_cli_args.preset, "fast");
    SAFE_STRCPY(g_cli_args.codec, "h264");
    SAFE_STRCPY(g_cli_args.path, "scenic");
    app_state_resolve_asset_path("assets/fonts/font.ttf", g_cli_args.log_font, sizeof(g_cli_args.log_font));
    SAFE_STRCPY(g_cli_args.log_color, "white");
    SAFE_STRCPY(g_cli_args.out, "mandelbrot_video.mp4");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            exit(0);
        } else if (strcmp(argv[i], "--headless") == 0) {
            g_cli_args.headless = 1;
        } else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            g_cli_args.width = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--height") == 0) && i + 1 < argc) {
            g_cli_args.height = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fps") == 0) && i + 1 < argc) {
            g_cli_args.fps = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--duration") == 0) && i + 1 < argc) {
            g_cli_args.duration = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--out") == 0) && i + 1 < argc) {
            SAFE_STRCPY(g_cli_args.out, argv[++i]);
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--crf") == 0) && i + 1 < argc) {
            g_cli_args.crf = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--path") == 0) && i + 1 < argc) {
            SAFE_STRCPY(g_cli_args.path, argv[++i]);
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--preset") == 0) && i + 1 < argc) {
            SAFE_STRCPY(g_cli_args.preset, argv[++i]);
        } else if (strcmp(argv[i], "--codec") == 0 && i + 1 < argc) {
            SAFE_STRCPY(g_cli_args.codec, argv[++i]);
        } else if (strcmp(argv[i], "--aa") == 0 && i + 1 < argc) {
            g_cli_args.aa = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log") == 0) {
            g_cli_args.log = 1;
        } else if (strcmp(argv[i], "--log-size") == 0 && i + 1 < argc) {
            g_cli_args.log_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-font") == 0 && i + 1 < argc) {
            SAFE_STRCPY(g_cli_args.log_font, argv[++i]);
        } else if (strcmp(argv[i], "--log-pos") == 0 && i + 1 < argc) {
            g_cli_args.log_pos = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-opacity") == 0 && i + 1 < argc) {
            g_cli_args.log_opacity = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--log-color") == 0 && i + 1 < argc) {
            SAFE_STRCPY(g_cli_args.log_color, argv[++i]);
        } else if (strcmp(argv[i], "--gpu") == 0) {
            g_cli_args.gpu = 1;
        } else if (strcmp(argv[i], "--curve") == 0 && i + 1 < argc) {
            g_cli_args.curve = atoi(argv[++i]);
        }
    }
    g_cli_args.parsed = 1;

    if (g_cli_args.headless) {
#ifdef __linux__
        if (g_cli_args.gpu) {
            run_headless_gpu_video_render();
            exit(0);
        }
#else
        if (g_cli_args.gpu) {
            fprintf(stderr, "warning: --gpu headless mode is only supported on Linux right now. Falling back to CPU.\n");
        }
#endif
        run_headless_video_render();
        exit(0);
    }

    return app_runner_get_desc(APP_BACKEND_VIDEO);
}
