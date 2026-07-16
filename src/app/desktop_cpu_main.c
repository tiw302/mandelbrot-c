/* 
 * [ARCH] platform entry point: desktop (software renderer)
 * 
 * we use separate entry point files for different build targets (cpu, gpu, web)
 * instead of massive #ifdef blocks. this keeps the cmake build targets clean
 * and ensures that platform-specific linkage (like opengl vs webgl) is isolated.
 * 
 * the actual application logic is centralized in app_runner.c.
 */
#include "app_runner.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    return app_runner_get_desc(APP_BACKEND_CPU);
}
