cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ensure consistent optimization flags without breaking 128-bit strict IEEE754 math
add_compile_options(-O3 -fno-math-errno -fno-signed-zeros -fno-trapping-math)

# strict compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wpedantic -Wno-unused-parameter -Wno-overlength-strings -Wno-unused-const-variable -Wno-unused-variable)
    # disable clang warnings for nan/infinity under fast-math and missing trailing newlines
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-nan-infinity-disabled)
        add_compile_options(-Wno-newline-eof)
    endif()
endif()

# --- build options ---
option(BUILD_CPU     "Build CPU-Engine (Desktop)"       OFF)
option(BUILD_GPU     "Build GPU-Engine (Desktop)"       OFF)
option(BUILD_WEB     "Build Web-Engine (WASM)"          OFF)
option(BUILD_DEEP    "Build Deep-Zoom Engine (Perturbation)" OFF)

# architecture tuning — set USE_NATIVE_ARCH=ON if your target machine
# matches the build machine. falls back to avx2+fma when OFF. if the
# target cpu lacks avx2, set ARCH_GENERIC=ON for a safe scalar build.
option(USE_NATIVE_ARCH "Use -march=native for host-optimal codegen" OFF)
option(ARCH_GENERIC    "Disable all SIMD for maximum portability"   OFF)

if(EMSCRIPTEN)
    # emscripten only supports -msimd128, not x86-specific -mavx2
    set(ARCH_FLAGS -msimd128)
elseif(ARCH_GENERIC)
    set(ARCH_FLAGS "")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
    # arm64 targets compile with built-in neon support, no x86-specific flags needed
    set(ARCH_FLAGS "")
elseif(USE_NATIVE_ARCH)
    set(ARCH_FLAGS -march=native)
else()
    set(ARCH_FLAGS -mavx2 -mfma -mpclmul)
endif()

# common core math
set(CORE_SRC src/core/mandelbrot.c src/core/julia.c src/core/burning_ship.c src/core/tricorn.c src/core/celtic.c src/core/buffalo.c src/core/color.c src/core/fractal.c src/core/bignum.c src/core/mandelbrot_bignum.c)

# common engine
set(ENGINE_SRC src/engine/renderer.c src/engine/tour.c src/engine/bookmark.c src/engine/config_loader.c src/engine/cjsonx_impl.c src/engine/camera.c src/engine/app_state.c src/engine/input_handler.c)

# --- 1a. static library definitions ---
add_library(mandelbrot_lib STATIC ${CORE_SRC} ${ENGINE_SRC})
target_compile_definitions(mandelbrot_lib PUBLIC USE_SIMD_F128)
target_compile_options(mandelbrot_lib PRIVATE ${ARCH_FLAGS})
target_include_directories(mandelbrot_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
)
target_include_directories(mandelbrot_lib SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/simd-f128
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjsonx
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sokol
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cimgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fons
)

# --- 1b. ui library definitions ---
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cimgui/imgui)
set(CIMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cimgui)
set(UI_SRC 
    ${IMGUI_DIR}/imgui.cpp 
    ${IMGUI_DIR}/imgui_draw.cpp 
    ${IMGUI_DIR}/imgui_tables.cpp 
    ${IMGUI_DIR}/imgui_widgets.cpp 
    ${IMGUI_DIR}/imgui_demo.cpp
    ${CIMGUI_DIR}/cimgui.cpp
    src/ui/imgui_impl.cc
)

add_library(mandelbrot_ui STATIC ${UI_SRC})
target_compile_features(mandelbrot_ui PRIVATE cxx_std_11)
target_include_directories(mandelbrot_ui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${IMGUI_DIR} 
    ${CIMGUI_DIR} 
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sokol
)

# helper for system libs
macro(find_system_libs target)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE X11 Xi Xcursor GL EGL dl pthread m)
    elseif(APPLE)
        target_link_libraries(${target} PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit -framework OpenGL")
    elseif(WIN32)
        target_link_libraries(${target} PRIVATE user32 gdi32 shell32 opengl32)
    endif()
endmacro()

# copy settings.json to the build directory so binaries can find it
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.json
    ${CMAKE_CURRENT_BINARY_DIR}/settings.json
    COPYONLY
)

# --- shader automation ---
set(GENERATED_SHADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_SHADERS_DIR})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/app/shaders.h
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile_shaders.sh
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mandelbrot.glsl ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile_shaders.sh
    COMMENT "Compiling shaders with sokol-shdc..."
)

set(GENERATED_SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/app/shaders.h)
add_custom_target(generate_shaders DEPENDS ${GENERATED_SHADERS})


# --- 1. cpu engine ---
if(BUILD_CPU)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

    if(APPLE)
        set_source_files_properties(src/app/app_runner.c src/app/desktop_cpu_main.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(mandelbrot_cpu src/engine/screenshot.c src/engine/perturbation.c src/ui/hud_sokol.c src/ui/hud_settings.c src/ui/hud_video_studio.c src/app/app_runner.c src/app/desktop_cpu_main.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_cpu PRIVATE SOKOL_GLCORE)
    target_compile_options(mandelbrot_cpu PRIVATE -pthread)
    target_include_directories(mandelbrot_cpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app ${GENERATED_SHADERS_DIR})
    find_system_libs(mandelbrot_cpu)
    target_link_libraries(mandelbrot_cpu PRIVATE mandelbrot_lib mandelbrot_ui)
endif()


# --- 2. gpu engine ---
if(BUILD_GPU)
    if(APPLE)
        # sokol_app.h uses cocoa on macos, so its implementation file must be compiled as objective-c
        set_source_files_properties(src/app/app_runner.c src/app/desktop_gpu_main.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(mandelbrot_gpu src/engine/perturbation.c src/engine/screenshot.c src/ui/hud_sokol.c src/ui/hud_settings.c src/ui/hud_video_studio.c src/app/app_runner.c src/app/desktop_gpu_main.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_gpu PRIVATE SOKOL_GLCORE BUILD_PERTURBATION=1)
    target_compile_options(mandelbrot_gpu PRIVATE -pthread)
    target_include_directories(mandelbrot_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app ${GENERATED_SHADERS_DIR})
    find_system_libs(mandelbrot_gpu)
    target_link_libraries(mandelbrot_gpu PRIVATE mandelbrot_lib mandelbrot_ui)
endif()


# --- 2b. deep / perturbation engine ---
if(BUILD_DEEP)
    if(APPLE)
        set_source_files_properties(src/app/app_runner.c src/app/desktop_gpu_main.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(mandelbrot_deep src/engine/perturbation.c src/engine/screenshot.c src/ui/hud_sokol.c src/ui/hud_settings.c src/ui/hud_video_studio.c src/app/app_runner.c src/app/desktop_gpu_main.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_deep PRIVATE SOKOL_GLCORE BUILD_PERTURBATION=1 BUILD_DEEP_TARGET=1)
    target_compile_options(mandelbrot_deep PRIVATE -pthread)
    target_include_directories(mandelbrot_deep PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app ${GENERATED_SHADERS_DIR})
    find_system_libs(mandelbrot_deep)
    target_link_libraries(mandelbrot_deep PRIVATE mandelbrot_lib mandelbrot_ui)
endif()


# --- 3. web engine ---
if(BUILD_WEB)
    add_executable(mandelbrot_web src/engine/perturbation.c src/engine/screenshot.c src/ui/hud_sokol.c src/ui/hud_settings.c src/ui/hud_video_studio.c src/app/app_runner.c src/app/web_main.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_web PRIVATE SOKOL_GLES3)
    target_compile_options(mandelbrot_web PRIVATE -O3 -flto -msimd128 -D__wasm_simd128__)
    target_include_directories(mandelbrot_web PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
    target_link_options(mandelbrot_web PRIVATE "-O3" "-flto" "-sWASM=1" "-sUSE_SDL=0" "-sUSE_WEBGL2=1" "-sFULL_ES3=1" "-sMINIMAL_RUNTIME=0" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=134217728" "-sMAXIMUM_MEMORY=1073741824")
    target_link_libraries(mandelbrot_web PRIVATE mandelbrot_lib mandelbrot_ui)
    set_target_properties(mandelbrot_web PROPERTIES OUTPUT_NAME "index")
endif()

# --- 4. link-time optimization (lto / ipo) ---
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    if(BUILD_CPU)
        set_target_properties(mandelbrot_cpu PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    if(BUILD_GPU)
        set_target_properties(mandelbrot_gpu PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    if(BUILD_DEEP)
        set_target_properties(mandelbrot_deep PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# --- 4. Standalone Video Renderer TUI ---
if(NOT EMSCRIPTEN)
    add_executable(mandelbrot_video src/engine/perturbation.c src/engine/screenshot.c src/ui/hud_sokol.c src/ui/hud_settings.c src/ui/hud_video_studio.c src/app/app_runner.c src/app/video_renderer_main.c src/app/headless_egl.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_video PRIVATE USE_SIMD_F128)
    target_compile_options(mandelbrot_video PRIVATE -pthread)
    target_include_directories(mandelbrot_video PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
    find_system_libs(mandelbrot_video)
    target_link_libraries(mandelbrot_video PRIVATE mandelbrot_lib mandelbrot_ui m)
endif()

# --- 5. benchmarks ---
# always add the benchmarks directory so it builds regardless of build_cpu/gpu, except under Emscripten
if(NOT EMSCRIPTEN)
    add_subdirectory(benchmarks)
endif()

# --- 6. tests ---
# add tests directory if build_testing is on
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
