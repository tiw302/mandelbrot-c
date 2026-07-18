/* app_runner.h
 *
 * central orchestrator for the application lifecycle.
 * configures and provides the sokol_app descriptor for different rendering
 * backends (cpu, gpu, web, video export) and manages their shared initialization.
 */

#pragma once

#if defined(__EMSCRIPTEN__)
#ifndef SOKOL_GLES3
#define SOKOL_GLES3
#endif
#else
#ifndef SOKOL_GLCORE
#define SOKOL_GLCORE
#endif
#endif

#include "core/math_types.h"
#include "sokol/sokol_app.h"

typedef enum { APP_BACKEND_CPU, APP_BACKEND_GPU, APP_BACKEND_WEB, APP_BACKEND_VIDEO } AppBackend;

// return the sokol_main app descriptor for the given mode
sapp_desc app_runner_get_desc(AppBackend mode);
