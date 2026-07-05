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

#include "sokol/sokol_app.h"

#include "core/math_types.h"

typedef enum {
    APP_BACKEND_CPU,
    APP_BACKEND_GPU,
    APP_BACKEND_WEB
} AppBackend;

// return the sokol_main app descriptor for the given mode
sapp_desc app_runner_get_desc(AppBackend mode);
