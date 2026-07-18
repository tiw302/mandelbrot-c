#if defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"

#ifndef SG_PIXELFORMAT_SBGR8A8
#define SG_PIXELFORMAT_SBGR8A8 44
#endif

#include "imgui.h"

#define SOKOL_IMGUI_IMPL
#include "sokol/sokol_imgui.h"
