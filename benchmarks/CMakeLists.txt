# benchmarks/CMakeLists.txt - Build configuration for benchmarks

# inherit compiler flags from parent (ARCH_FLAGS)
set(BENCHMARK_FLAGS "${ARCH_FLAGS}")

# 1. benchmark_math (performance benchmarking for math kernels)
add_executable(benchmark_math cpu/benchmark_math.c)
target_compile_options(benchmark_math PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_math PRIVATE mandelbrot_lib m)

# 2. benchmark_renderer (performance benchmarking for multithreaded rendering)
add_executable(benchmark_renderer cpu/benchmark_renderer.c)
target_compile_options(benchmark_renderer PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_renderer PRIVATE mandelbrot_lib m pthread)

# 3. benchmark_screenshot (performance benchmarking for image I/O and export)
add_executable(benchmark_screenshot cpu/benchmark_screenshot.c ../src/engine/screenshot.c ../src/engine/tour.c)
target_compile_options(benchmark_screenshot PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_screenshot PRIVATE mandelbrot_lib m pthread)

# 4. benchmark_gpu (performance benchmarking for sokol GL GPU pipeline)
if(BUILD_GPU)
    if(APPLE)
        set_source_files_properties(gpu/benchmark_gpu.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    add_executable(benchmark_gpu gpu/benchmark_gpu.c)
    target_compile_definitions(benchmark_gpu PRIVATE SOKOL_GLCORE FORCE_GPU_MODE)
    target_compile_options(benchmark_gpu PRIVATE ${ARCH_FLAGS} -pthread)
    target_include_directories(benchmark_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/app)
    target_link_libraries(benchmark_gpu PRIVATE mandelbrot_lib)
    
    # use the macro defined in the root CMakeLists.txt to link correct platform GL/windowing libs
    find_system_libs(benchmark_gpu)
endif()
