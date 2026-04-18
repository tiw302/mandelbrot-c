cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_WEB "Build for Web (Sokol/WASM)" OFF)
option(BUILD_DESKTOP "Build for Desktop (SDL2)" ON)

# Global Include Directories
include_directories(
    include
    src/core
    third_party
    third_party/sokol
    third_party/stb
    third_party/fons
)

# --- Core Logic ---
set(CORE_SRC 
    src/core/mandelbrot.c 
    src/core/julia.c 
    src/core/color.c
)

# --- Desktop Target (SDL2) ---
if(BUILD_DESKTOP AND NOT BUILD_WEB)
    set(DESKTOP_SRC 
        src/platform/desktop/main.c 
        src/platform/desktop/renderer.c 
        src/platform/desktop/screenshot.c 
        src/platform/desktop/tour.c
    )
    
    find_package(SDL2 REQUIRED)
    find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf REQUIRED)
    find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2 REQUIRED)

    add_executable(mandelbrot-desktop ${CORE_SRC} ${DESKTOP_SRC})
    target_compile_options(mandelbrot-desktop PRIVATE -mavx2 -mfma -pthread)
    target_include_directories(mandelbrot-desktop PRIVATE src/platform/desktop ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIR})
    target_link_libraries(mandelbrot-desktop PRIVATE ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARY} m z png pthread)
endif()

# --- Web Target (Sokol GFX) ---
if(BUILD_WEB)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "Use emcmake to build for Web")
    endif()

    set(WEB_SRC 
        src/platform/web/main_web.c 
        src/platform/web/renderer_web.c
        src/platform/desktop/renderer.c 
        src/platform/desktop/tour.c
    )

    add_executable(mandelbrot-web ${CORE_SRC} ${WEB_SRC})
    target_include_directories(mandelbrot-web PRIVATE src/platform/web src/platform/desktop)
    
    target_compile_options(mandelbrot-web PRIVATE -msimd128 -D__wasm_simd128__)
    target_link_options(mandelbrot-web PRIVATE 
        "-s WASM=1"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s USE_SDL=0"
        "-s MAX_WEBGL_VERSION=2"
        "-pthread"
        "-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
        "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
    )
    set_target_properties(mandelbrot-web PROPERTIES OUTPUT_NAME "index")
endif()
