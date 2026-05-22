cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- build options ---
option(BUILD_CPU     "Build CPU-Engine (Desktop)"       OFF)
option(BUILD_CPU_128 "Build CPU-Engine 128-bit (Desktop)" OFF)
option(BUILD_GPU     "Build GPU-Engine (Desktop)"       OFF)
option(BUILD_WEB     "Build Web-Engine (WASM)"          OFF)

# architecture tuning — set USE_NATIVE_ARCH=ON if your target machine
# matches the build machine. falls back to avx2+fma when OFF. if the
# target cpu lacks avx2, set ARCH_GENERIC=ON for a safe scalar build.
option(USE_NATIVE_ARCH "Use -march=native for host-optimal codegen" OFF)
option(ARCH_GENERIC    "Disable all SIMD for maximum portability"   OFF)

if(ARCH_GENERIC)
    set(ARCH_FLAGS "")
elseif(USE_NATIVE_ARCH)
    set(ARCH_FLAGS -march=native)
else()
    set(ARCH_FLAGS -mavx2 -mfma)
endif()

# include paths
include_directories(include src/engine src/app third_party third_party/sokol third_party/stb third_party/fons third_party/simd-f128)

# common core math
set(CORE_SRC src/core/mandelbrot.c src/core/julia.c src/core/burning_ship.c src/core/color.c)

# helper for system libs
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
    add_executable(mandelbrot_cpu ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/engine/screenshot.c src/engine/bookmark.c src/engine/ini_config.c src/app/desktop_cpu_main.c)
    target_compile_definitions(mandelbrot_cpu PRIVATE SOKOL_GLCORE USE_SIMD_F128)
    target_compile_options(mandelbrot_cpu PRIVATE ${ARCH_FLAGS} -pthread)
    find_system_libs(mandelbrot_cpu)
    target_link_libraries(mandelbrot_cpu PRIVATE SDL2 SDL2_ttf m)
endif()

# --- 1.5. CPU Engine (128-bit) ---
if(BUILD_CPU_128)
    add_executable(mandelbrot_cpu_128 ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/engine/screenshot.c src/engine/bookmark.c src/engine/ini_config.c src/app/desktop_cpu_main.c)
    target_compile_definitions(mandelbrot_cpu_128 PRIVATE SOKOL_GLCORE USE_SIMD_F128)
    target_compile_options(mandelbrot_cpu_128 PRIVATE ${ARCH_FLAGS} -pthread)
    find_system_libs(mandelbrot_cpu_128)
    target_link_libraries(mandelbrot_cpu_128 PRIVATE SDL2 SDL2_ttf m)
endif()

# --- 2. GPU Engine ---
if(BUILD_GPU)
    add_executable(mandelbrot_gpu ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/engine/screenshot.c src/engine/bookmark.c src/engine/ini_config.c src/app/desktop_gpu_main.c)
    target_compile_definitions(mandelbrot_gpu PRIVATE SOKOL_GLCORE FORCE_GPU_MODE USE_SIMD_F128)
    target_compile_options(mandelbrot_gpu PRIVATE ${ARCH_FLAGS} -pthread)
    find_system_libs(mandelbrot_gpu)
endif()

# --- 3. Web Engine ---
if(BUILD_WEB)
    add_executable(mandelbrot_web ${CORE_SRC} src/engine/renderer.c src/engine/tour.c src/engine/bookmark.c src/engine/ini_config.c src/app/web_main.c)
    target_compile_definitions(mandelbrot_web PRIVATE SOKOL_GLES3)
    target_compile_options(mandelbrot_web PRIVATE -O3 -flto -msimd128 -D__wasm_simd128__ -pthread)
    target_link_options(mandelbrot_web PRIVATE "-O3" "-flto" "-sWASM=1" "-sUSE_SDL=0" "-sUSE_WEBGL2=1" "-sFULL_ES3=1" "-sMINIMAL_RUNTIME=0" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=134217728" "-sMAXIMUM_MEMORY=1073741824" "-pthread")
    set_target_properties(mandelbrot_web PROPERTIES OUTPUT_NAME "index")
endif()
