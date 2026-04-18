cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

# Set C Standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_WEB "Build for Web (WASM)" OFF)
option(BUILD_GPU "Build for GPU (CUDA)" OFF)

# Common Compiler Flags
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-O3 -Wall -Wextra)
endif()

# Common Include Directories
include_directories(include core)

# Source Files
set(CORE_SRC 
    core/mandelbrot.c 
    core/julia.c 
    core/color.c
)

# --- Desktop CPU Build ---
if(NOT BUILD_WEB AND NOT BUILD_GPU)
    set(CPU_SRC 
        cpu-engine/main.c 
        cpu-engine/renderer.c 
        cpu-engine/screenshot.c 
        cpu-engine/tour.c
    )
    
    # Platform specific flags
    if(NOT MSVC)
        add_compile_options(-mavx2 -mfma -pthread)
    endif()

    # Find SDL2
    find_package(SDL2 REQUIRED)
    # Find SDL2_ttf (using find_library as it might not have a config file)
    find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf REQUIRED)
    find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2 REQUIRED)

    add_executable(mandelbrot-desktop ${CORE_SRC} ${CPU_SRC})
    
    target_include_directories(mandelbrot-desktop PRIVATE cpu-engine ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIR})
    target_link_libraries(mandelbrot-desktop PRIVATE ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARY} m z png pthread)
endif()

# --- Web (Emscripten) Build ---
if(BUILD_WEB)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "You must use emcmake to build for Web")
    endif()

    set(WASM_SRC 
        web-engine/main_wasm.c 
        web-engine/renderer_wasm.c
        cpu-engine/renderer.c 
        cpu-engine/tour.c
    )

    add_executable(mandelbrot-web ${CORE_SRC} ${WASM_SRC})
    
    target_include_directories(mandelbrot-web PRIVATE web-engine)
    
    set_target_properties(mandelbrot-web PROPERTIES OUTPUT_NAME "mandelbrot")
    
    target_compile_options(mandelbrot-web PRIVATE -msimd128 -D__wasm_simd128__)
    target_link_options(mandelbrot-web PRIVATE 
        "-s WASM=1"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s MAX_WEBGL_VERSION=2"
        "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-pthread"
        "-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
        "--use-port=sdl2"
        "--use-port=sdl2_ttf"
    )
endif()

# --- GPU (CUDA) Build ---
if(BUILD_GPU)
    enable_language(CUDA)
    
    set(GPU_SRC 
        gpu-engine/mandelbrot_kernel.cu
    )
    
    add_executable(mandelbrot-gpu ${CORE_SRC} ${GPU_SRC})
    target_include_directories(mandelbrot-gpu PRIVATE gpu-engine)
endif()
