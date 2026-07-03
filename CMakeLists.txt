cmake_minimum_required(VERSION 3.10)
project(MandelbrotC C)

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
set(CORE_SRC src/core/mandelbrot.c src/core/julia.c src/core/burning_ship.c src/core/tricorn.c src/core/celtic.c src/core/buffalo.c src/core/color.c src/core/fractal.c)

# common engine
set(ENGINE_SRC src/engine/renderer.c src/engine/tour.c src/engine/bookmark.c src/engine/ini_config.c src/engine/cjsonx_impl.c src/engine/camera.c src/engine/app_state.c src/engine/input_handler.c)

# --- 1a. static library definitions ---
add_library(mandelbrot_lib STATIC ${CORE_SRC} ${ENGINE_SRC})
target_compile_definitions(mandelbrot_lib PUBLIC USE_SIMD_F128)
target_compile_options(mandelbrot_lib PRIVATE ${ARCH_FLAGS})
target_include_directories(mandelbrot_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(mandelbrot_lib SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/simd-f128
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjsonx
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sokol
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fons
)

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

# --- shader automation ---
set(GENERATED_SHADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_SHADERS_DIR})

add_custom_command(
    OUTPUT ${GENERATED_SHADERS_DIR}/desktop_gpu_vs.h
    COMMAND ${CMAKE_COMMAND} -DGLSL_FILE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/desktop_gpu_vs.glsl -DHEADER_FILE=${GENERATED_SHADERS_DIR}/desktop_gpu_vs.h -DVAR_NAME=dg_vs -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_shader.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/desktop_gpu_vs.glsl ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_shader.cmake
    COMMENT "Embedding vertex shader..."
)

add_custom_command(
    OUTPUT ${GENERATED_SHADERS_DIR}/desktop_gpu_fs_cpu.h
    COMMAND ${CMAKE_COMMAND} -DGLSL_FILE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/desktop_gpu_fs_cpu.glsl -DHEADER_FILE=${GENERATED_SHADERS_DIR}/desktop_gpu_fs_cpu.h -DVAR_NAME=dg_fs_cpu -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_shader.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/desktop_gpu_fs_cpu.glsl ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_shader.cmake
    COMMENT "Embedding CPU display fragment shader..."
)

add_custom_command(
    OUTPUT ${GENERATED_SHADERS_DIR}/desktop_gpu_fs_gpu.h
    COMMAND ${CMAKE_COMMAND} -DGLSL_FILE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/desktop_gpu_fs_gpu.glsl -DHEADER_FILE=${GENERATED_SHADERS_DIR}/desktop_gpu_fs_gpu.h -DVAR_NAME=dg_fs_gpu -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_shader.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/desktop_gpu_fs_gpu.glsl ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_shader.cmake
    COMMENT "Embedding GPU fragment shader..."
)

set(GENERATED_SHADERS
    ${GENERATED_SHADERS_DIR}/desktop_gpu_vs.h
    ${GENERATED_SHADERS_DIR}/desktop_gpu_fs_cpu.h
    ${GENERATED_SHADERS_DIR}/desktop_gpu_fs_gpu.h
)

# --- 1. cpu engine ---
if(BUILD_CPU)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

    add_executable(mandelbrot_cpu src/engine/screenshot.c src/ui/hud_sdl.c src/ui/settings_panel_sdl.c src/app/desktop_cpu_main.c)
    target_compile_definitions(mandelbrot_cpu PRIVATE SOKOL_GLCORE USE_SDL2)
    target_compile_options(mandelbrot_cpu PRIVATE -pthread)
    target_include_directories(mandelbrot_cpu PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
    target_link_directories(mandelbrot_cpu PRIVATE ${SDL2_LIBRARY_DIRS} ${SDL2_TTF_LIBRARY_DIRS})
    find_system_libs(mandelbrot_cpu)
    target_link_libraries(mandelbrot_cpu PRIVATE mandelbrot_lib ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES} m)
endif()


# --- 2. gpu engine ---
if(BUILD_GPU)
    if(APPLE)
        # sokol_app.h uses cocoa on macos, so its implementation file must be compiled as objective-c
        set_source_files_properties(src/app/desktop_gpu_main.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(mandelbrot_gpu src/engine/perturbation.c src/engine/screenshot.c src/ui/hud_sokol.c src/ui/settings_panel.c src/app/desktop_gpu_main.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_gpu PRIVATE SOKOL_GLCORE BUILD_PERTURBATION=1)
    target_compile_options(mandelbrot_gpu PRIVATE -pthread)
    target_include_directories(mandelbrot_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app ${GENERATED_SHADERS_DIR})
    find_system_libs(mandelbrot_gpu)
    target_link_libraries(mandelbrot_gpu PRIVATE mandelbrot_lib)
endif()


# --- 2b. deep / perturbation engine ---
if(BUILD_DEEP)
    if(APPLE)
        set_source_files_properties(src/app/desktop_gpu_main.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(mandelbrot_deep src/engine/perturbation.c src/engine/screenshot.c src/ui/hud_sokol.c src/ui/settings_panel.c src/app/desktop_gpu_main.c ${GENERATED_SHADERS})
    target_compile_definitions(mandelbrot_deep PRIVATE SOKOL_GLCORE BUILD_PERTURBATION=1)
    target_compile_options(mandelbrot_deep PRIVATE -pthread)
    target_include_directories(mandelbrot_deep PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app ${GENERATED_SHADERS_DIR})
    find_system_libs(mandelbrot_deep)
    target_link_libraries(mandelbrot_deep PRIVATE mandelbrot_lib)
endif()


# --- 3. web engine ---
if(BUILD_WEB)
    add_executable(mandelbrot_web src/app/web_main.c)
    target_compile_definitions(mandelbrot_web PRIVATE SOKOL_GLES3)
    target_compile_options(mandelbrot_web PRIVATE -O3 -flto -msimd128 -D__wasm_simd128__)
    target_include_directories(mandelbrot_web PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app)
    target_link_options(mandelbrot_web PRIVATE "-O3" "-flto" "-sWASM=1" "-sUSE_SDL=0" "-sUSE_WEBGL2=1" "-sFULL_ES3=1" "-sMINIMAL_RUNTIME=0" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=134217728" "-sMAXIMUM_MEMORY=1073741824")
    target_link_libraries(mandelbrot_web PRIVATE mandelbrot_lib)
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
