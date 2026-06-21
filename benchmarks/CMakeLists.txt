# benchmarks/CMakeLists.txt - Build configuration for benchmarks

# shared include directories for all benchmarks
include_directories(../src/core ../src/engine ../src/app ../include ../third_party ../third_party/sokol ../third_party/stb ../third_party/fons ../third_party/simd-f128)

# inherit compiler flags from parent (ARCH_FLAGS)
set(BENCHMARK_FLAGS "${ARCH_FLAGS}")

# 1. benchmark_math (performance benchmarking for math kernels)
add_executable(benchmark_math cpu/benchmark_math.c ../src/core/mandelbrot.c ../src/core/julia.c ../src/core/burning_ship.c)
target_compile_definitions(benchmark_math PRIVATE USE_SIMD_F128)
target_compile_options(benchmark_math PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_math PRIVATE m)

# 2. benchmark_renderer (performance benchmarking for multithreaded rendering)
add_executable(benchmark_renderer cpu/benchmark_renderer.c
    ../src/engine/renderer.c
    ../src/engine/ini_config.c
    ../src/core/color.c
    ../src/core/mandelbrot.c
    ../src/core/julia.c
    ../src/core/burning_ship.c
    ../src/core/fractal.c
)
target_compile_definitions(benchmark_renderer PRIVATE USE_SIMD_F128)
target_compile_options(benchmark_renderer PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_renderer PRIVATE m pthread)

# 3. benchmark_screenshot (performance benchmarking for image I/O and export)
add_executable(benchmark_screenshot cpu/benchmark_screenshot.c
    ../src/engine/screenshot.c
    ../src/engine/renderer.c
    ../src/engine/ini_config.c
    ../src/core/color.c
    ../src/core/mandelbrot.c
    ../src/core/julia.c
    ../src/core/burning_ship.c
    ../src/core/fractal.c
)
target_compile_definitions(benchmark_screenshot PRIVATE USE_SIMD_F128)
target_compile_options(benchmark_screenshot PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_screenshot PRIVATE m pthread)

# 4. benchmark_gpu (performance benchmarking for sokol GL GPU pipeline)
if(BUILD_GPU)
    if(APPLE)
        set_source_files_properties(gpu/benchmark_gpu.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(benchmark_gpu gpu/benchmark_gpu.c
        ../src/core/color.c
        ../src/core/mandelbrot.c
        ../src/core/julia.c
        ../src/core/burning_ship.c
        ../src/core/fractal.c
        ../src/engine/ini_config.c
    )
    target_compile_definitions(benchmark_gpu PRIVATE SOKOL_GLCORE FORCE_GPU_MODE USE_SIMD_F128)
    target_compile_options(benchmark_gpu PRIVATE ${ARCH_FLAGS} -pthread)
    
    # use the macro defined in the root CMakeLists.txt to link correct platform GL/windowing libs
    find_system_libs(benchmark_gpu)
endif()
