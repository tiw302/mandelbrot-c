# benchmarks/CMakeLists.txt - Build configuration for benchmarks

# inherit compiler flags from parent (ARCH_FLAGS)
set(BENCHMARK_FLAGS "${ARCH_FLAGS}")

# 1. benchmark_math (performance benchmarking for math kernels)
add_executable(benchmark_math cpu/benchmark_math.c)
target_compile_options(benchmark_math PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_math PRIVATE mandelbrot_lib m)

# 2. benchmark_fractal_types (performance comparing fractal equations)
add_executable(benchmark_fractal_types cpu/benchmark_fractal_types.c)
target_compile_options(benchmark_fractal_types PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_fractal_types PRIVATE mandelbrot_lib m)

# 3. benchmark_renderer (performance benchmarking for multithreaded rendering)
add_executable(benchmark_renderer cpu/benchmark_renderer.c)
target_compile_options(benchmark_renderer PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_renderer PRIVATE mandelbrot_lib m pthread)

# 3. benchmark_screenshot (performance benchmarking for image I/O and export)
add_executable(benchmark_screenshot cpu/benchmark_screenshot.c)
target_compile_options(benchmark_screenshot PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_screenshot PRIVATE mandelbrot_lib m pthread)

# 4. benchmark_perturbation (performance benchmarking for perturbation theory ref orbit)
add_executable(benchmark_perturbation cpu/benchmark_perturbation.c)
target_compile_options(benchmark_perturbation PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_perturbation PRIVATE mandelbrot_lib m)

# 5. benchmark_bignum (performance benchmarking for bignum high precision)
add_executable(benchmark_bignum cpu/benchmark_bignum.c)
target_compile_options(benchmark_bignum PRIVATE ${BENCHMARK_FLAGS} -O3 -ffast-math)
target_link_libraries(benchmark_bignum PRIVATE mandelbrot_lib m)

# 6. benchmark_gpu (performance benchmarking for sokol GL GPU pipeline)
if(BUILD_GPU)
    if(APPLE)
        set_source_files_properties(gpu/benchmark_gpu.c PROPERTIES COMPILE_FLAGS "-x objective-c")
    endif()
    
    add_executable(benchmark_gpu gpu/benchmark_gpu.c)
    add_dependencies(benchmark_gpu generate_shaders)
    
    target_compile_definitions(benchmark_gpu PRIVATE SOKOL_GLCORE FORCE_GPU_MODE)
    target_compile_options(benchmark_gpu PRIVATE ${ARCH_FLAGS} -pthread)
    target_include_directories(benchmark_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/app ${CMAKE_BINARY_DIR}/generated)
    target_link_libraries(benchmark_gpu PRIVATE mandelbrot_lib)
    
    # use the macro defined in the root CMakeLists.txt to link correct platform GL/windowing libs
    find_system_libs(benchmark_gpu)
endif()
