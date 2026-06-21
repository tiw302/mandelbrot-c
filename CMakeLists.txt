cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ensure consistent optimization flags
add_compile_options(-O3 -ffast-math)

# --- build options ---
option(BUILD_CPU     "Build CPU-Engine (Desktop)"       OFF)
option(BUILD_GPU     "Build GPU-Engine (Desktop)"       OFF)
option(BUILD_WEB     "Build Web-Engine (WASM)"          OFF)
option(BUILD_DEEP    "Build Deep-Zoom Engine (GMP)"     OFF)

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
elseif(USE_NATIVE_ARCH)
    set(ARCH_FLAGS -march=native)
else()
    set(ARCH_FLAGS -mavx2 -mfma)
endif()

# include paths
include_directories(include src/engine src/app third_party third_party/sokol third_party/stb third_party/fons third_party/simd-f128)

# common core math
set(CORE_SRC src/core/mandelbrot.c src/core/julia.c src/core/burning_ship.c src/core/color.c src/core/fractal.c)

# common engine
set(ENGINE_SRC src/engine/renderer.c src/engine/tour.c src/engine/bookmark.c src/engine/ini_config.c)

# helper for system libs
macro(find_system_libs target)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target} PRIVATE X11 Xi Xcursor GL dl pthread m)
    elseif(APPLE)
        target_link_libraries(${target} PRIVATE "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit -framework OpenGL")
    elseif(WIN32)
        target_link_libraries(${target} PRIVATE user32 gdi32 shell32 opengl32)
    endif()
endmacro()

# --- 1. cpu engine ---
if(BUILD_CPU)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

    add_executable(mandelbrot_cpu ${CORE_SRC} ${ENGINE_SRC} src/engine/screenshot.c src/app/desktop_cpu_main.c)
    target_compile_definitions(mandelbrot_cpu PRIVATE SOKOL_GLCORE USE_SIMD_F128)
    target_compile_options(mandelbrot_cpu PRIVATE ${ARCH_FLAGS} -pthread)
    target_include_directories(mandelbrot_cpu PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    target_link_directories(mandelbrot_cpu PRIVATE ${SDL2_LIBRARY_DIRS} ${SDL2_TTF_LIBRARY_DIRS})
    find_system_libs(mandelbrot_cpu)
    target_link_libraries(mandelbrot_cpu PRIVATE ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES} m)
endif()


# --- 2. gpu engine ---
if(BUILD_GPU)
    if(APPLE)
        # sokol_app.h uses cocoa on macos, so its implementation file must be compiled as objective-c
        set_source_files_properties(src/app/desktop_gpu_main.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(mandelbrot_gpu ${CORE_SRC} ${ENGINE_SRC} src/engine/screenshot.c src/app/desktop_gpu_main.c)
    target_compile_definitions(mandelbrot_gpu PRIVATE SOKOL_GLCORE FORCE_GPU_MODE USE_SIMD_F128)
    target_compile_options(mandelbrot_gpu PRIVATE ${ARCH_FLAGS} -pthread)
    find_system_libs(mandelbrot_gpu)
endif()

# --- 3. web engine ---
if(BUILD_WEB)
    add_executable(mandelbrot_web ${CORE_SRC} ${ENGINE_SRC} src/app/web_main.c)
    target_compile_definitions(mandelbrot_web PRIVATE SOKOL_GLES3 USE_SIMD_F128)
    target_compile_options(mandelbrot_web PRIVATE -O3 -flto -msimd128 -D__wasm_simd128__)
    target_link_options(mandelbrot_web PRIVATE "-O3" "-flto" "-sWASM=1" "-sUSE_SDL=0" "-sUSE_WEBGL2=1" "-sFULL_ES3=1" "-sMINIMAL_RUNTIME=0" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=134217728" "-sMAXIMUM_MEMORY=1073741824")
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
endif()

# --- 5. benchmarks ---
# always add the benchmarks directory so it builds regardless of build_cpu/gpu
add_subdirectory(benchmarks)

# --- 6. tests ---
# add tests directory if build_testing is on
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
