cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_WEB "Build for Web" OFF)

# Global Include Directories
include_directories(
    include
    src/core
    src/platform/desktop
    third_party
    third_party/sokol
    third_party/stb
    third_party/fons
)

# Common Source Files
set(CORE_SRC 
    src/core/mandelbrot.c 
    src/core/julia.c 
    src/core/color.c
)

set(COMMON_PLATFORM_SRC
    src/platform/desktop/renderer.c
    src/platform/desktop/tour.c
    src/platform/desktop/screenshot.c
    src/platform/universal_main.c
)

# --- Native Desktop (Sokol GFX) ---
if(NOT BUILD_WEB)
    if(UNIX AND NOT APPLE)
        set(SYS_LIBS X11 Xi Xcursor GL dl pthread m)
    elseif(APPLE)
        set(SYS_LIBS "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit")
    elseif(WIN32)
        set(SYS_LIBS user32 gdi32 shell32)
    endif()

    add_executable(mandelbrot-desktop ${CORE_SRC} ${COMMON_PLATFORM_SRC})
    target_compile_options(mandelbrot-desktop PRIVATE -mavx2 -mfma -pthread)
    target_link_libraries(mandelbrot-desktop PRIVATE ${SYS_LIBS})
endif()

# --- Web Target (Sokol GFX) ---
if(BUILD_WEB)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "Use emcmake for web build")
    endif()

    add_executable(mandelbrot-web ${CORE_SRC} ${COMMON_PLATFORM_SRC})
    target_compile_options(mandelbrot-web PRIVATE -msimd128 -D__wasm_simd128__)
    target_link_options(mandelbrot-web PRIVATE 
        "-s WASM=1"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s MAX_WEBGL_VERSION=2"
        "-pthread"
        "-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
        "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
    )
    set_target_properties(mandelbrot-web PROPERTIES OUTPUT_NAME "index")
endif()
