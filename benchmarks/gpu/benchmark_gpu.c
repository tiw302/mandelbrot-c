/* benchmark_gpu.c
 *
 * automated gpu performance benchmark using sokol gfx.
 * measures fill rate and fragment shader execution speeds.
 */

#define SOKOL_IMPL
#define SOKOL_APP_IMPL
#define SOKOL_GFX_IMPL
#define SOKOL_GLUE_IMPL
#define SOKOL_TIME_IMPL

// clang-format off
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
// clang-format on

#include <stdio.h>
#include <stdlib.h>

#include "desktop_gpu_vs.h"
#include "desktop_gpu_fs_gpu.h"

#define BENCHMARK_FRAMES 200

// mirrors the glsl layout exactly to ensure correct memory mapping.
typedef struct {
    float center_hi[2];
    float center_lo[2];
    float julia_c_hi[2];
    float julia_c_lo[2];
    float zoom, iters, aspect;
    float fractal_type, palette, high_precision;
} params_t;

static struct {
    sg_pipeline pip_gpu;
    sg_bindings bind;
    sg_pass_action pass_action;
    int win_w, win_h;
    uint64_t start_time;
    int frame_count;
} state;

static void init(void) {
    sg_setup(&(sg_desc){.environment = sglue_environment()});
    stm_setup();

    state.win_w = sapp_width();
    state.win_h = sapp_height();

    // full-screen quad for fractal rendering
    float verts[] = {-1, 1, 0, 0, 1, 1, 1, 0, 1, -1, 1, 1, -1, -1, 0, 1};
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(verts)});
    uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.index_buffer = true, .immutable = true}, .data = SG_RANGE(idx)});

    // build gpu compute pipeline
    sg_shader shd_gpu = sg_make_shader(&(sg_shader_desc){
        .attrs[0].glsl_name = "pos",
        .attrs[1].glsl_name = "uv_in",
        .vertex_func.source = dg_vs,
        .fragment_func.source = dg_fs_gpu,
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .size = sizeof(params_t),
            .glsl_uniforms = {{.glsl_name = "u_center_hi", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_center_lo", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_julia_c_hi", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_julia_c_lo", .type = SG_UNIFORMTYPE_FLOAT2},
                              {.glsl_name = "u_zoom", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_iters", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_aspect", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_fractal_type", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_palette", .type = SG_UNIFORMTYPE_FLOAT},
                              {.glsl_name = "u_high_precision", .type = SG_UNIFORMTYPE_FLOAT}}}});
    state.pip_gpu =
        sg_make_pipeline(&(sg_pipeline_desc){.shader = shd_gpu,
                                             .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2,
                                             .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2,
                                             .index_type = SG_INDEXTYPE_UINT16});

    state.pass_action = (sg_pass_action){
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0, 0, 0, 1}}};

    printf("==========================================\n");
    printf(" GPU Benchmark (Sokol GL)\n");
    printf(" Resolution: %d x %d\n", state.win_w, state.win_h);
    printf(" Frames: %d\n", BENCHMARK_FRAMES);
    printf(" Max Iterations: 1000\n");
    printf("==========================================\n");
    printf("Running...\n");

    state.frame_count = 0;
    state.start_time = stm_now();
}

static void frame(void) {
    if (state.frame_count >= BENCHMARK_FRAMES) {
        double elapsed = stm_sec(stm_diff(stm_now(), state.start_time));
        double fps = BENCHMARK_FRAMES / elapsed;
        double mpx_s = ((double)state.win_w * state.win_h * BENCHMARK_FRAMES / 1e6) / elapsed;

        printf("\nResults:\n");
        printf("  -> Total time: %.4f seconds\n", elapsed);
        printf("  -> Average FPS:  %.2f fps\n", fps);
        printf("  -> Speed:        %.2f Mpx/s\n", mpx_s);
        printf("Benchmark complete.\n");

        sapp_quit();
        return;
    }

    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state.pip_gpu);
    sg_apply_bindings(&state.bind);

    params_t p = {
        .center_hi = {-0.5f, 0.0f},
        .center_lo = {0.0f, 0.0f},
        .julia_c_hi = {0.0f, 0.0f},
        .julia_c_lo = {0.0f, 0.0f},
        .zoom = 2.0f,
        .iters = 1000.0f,
        .aspect = (float)state.win_w / state.win_h,
        .fractal_type = 0.0f,  // mandelbrot
        .palette = 0.0f,
        .high_precision = 0.0f  // 32-bit test for now
    };
    sg_apply_uniforms(0, &SG_RANGE(p));
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();

    state.frame_count++;
}

static void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){.init_cb = init,
                       .frame_cb = frame,
                       .cleanup_cb = cleanup,
                       .width = 1920,
                       .height = 1080,
                       .window_title = "GPU Benchmark",
                       .high_dpi = false,
                       .swap_interval = 0};
}
