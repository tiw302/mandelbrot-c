cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options to enable/disable engines
option(BUILD_CPU "Build CPU-Engine (Desktop)" OFF)
option(BUILD_GPU "Build GPU-Engine (Desktop)" OFF)
option(BUILD_WEB "Build Web-Engine (WASM)" OFF)

# Include paths
# This should make headers like fontstash.h accessible from third_party/fons/
include_directories(include src/engine src/app third_party third_party/sokol third_party/stb third_party/fons)

# Common core math
set(CORE_SRC src/core/mandelbrot.c src/core/julia.c src/core/color.c)

# Helper for system libs
macro(find_system_libs target)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE X11 Xi Xcursor GL dl pthread m)
    elseif(APPLE)
        target_link_libraries(${target} PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
    elseif(WIN32)
        target_link_libraries(${target} PRIVATE user32 gdi32 shell32)
    endif()
endmacro()

# --- 1. CPU Engine ---
if(BUILD_CPU)
    # Ensure web_main.c is NOT included here. Added desktop_cpu_main.c for CPU.
    add_executable(mandelbrot-cpu ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/engine/screenshot.c src/app/desktop_cpu_main.c)
    target_compile_definitions(mandelbrot-cpu PRIVATE SOKOL_GLCORE)
    target_compile_options(mandelbrot-cpu PRIVATE -mavx2 -mfma -pthread)
    find_system_libs(mandelbrot-cpu)
    target_link_libraries(mandelbrot-cpu PRIVATE SDL2 SDL2_ttf m)
endif()

# --- 2. GPU Engine ---
if(BUILD_GPU)
    add_executable(mandelbrot-gpu ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/engine/screenshot.c src/app/desktop_gpu_main.c)
    target_compile_definitions(mandelbrot-gpu PRIVATE SOKOL_GLCORE FORCE_GPU_MODE)
    target_compile_options(mandelbrot-gpu PRIVATE -mavx2 -mfma -pthread)
    find_system_libs(mandelbrot-gpu)
endif()

# --- 3. Web Engine ---
if(BUILD_WEB)
    # Ensure only web-specific sources are included here. Headers are handled by include_directories and #include directives.
    add_executable(mandelbrot-web ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/app/web_main.c)
    target_compile_definitions(mandelbrot-web PRIVATE SOKOL_GLES3)
    target_compile_options(mandelbrot-web PRIVATE -O3 -flto -msimd128 -D__wasm_simd128__ -pthread)
    target_link_options(mandelbrot-web PRIVATE "-O3" "-flto" "-sWASM=1" "-sUSE_SDL=0" "-sUSE_WEBGL2=1" "-sFULL_ES3=1" "-sMINIMAL_RUNTIME=0" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=67108864" "-pthread")
    set_target_properties(mandelbrot-web PROPERTIES OUTPUT_NAME "index")
endif()
