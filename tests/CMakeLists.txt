# tests/CMakeLists.txt - Build configuration for unit tests

# allow testing
enable_testing()

# shared include directories for all tests
include_directories(../src/core ../src/engine ../src/app ../include ../third_party/simd-f128)

# inherit compiler flags from parent (ARCH_FLAGS)
set(TEST_FLAGS "${ARCH_FLAGS}")

# 1. test_math (core math kernels)
add_executable(test_math test_math.c ../src/core/mandelbrot.c ../src/core/julia.c ../src/core/burning_ship.c)
target_compile_options(test_math PRIVATE ${TEST_FLAGS})
target_link_libraries(test_math PRIVATE m)
add_test(NAME math_kernels COMMAND test_math)

# 2. test_color (palette logic)
add_executable(test_color test_color.c ../src/core/color.c)
target_compile_options(test_color PRIVATE ${TEST_FLAGS})
target_link_libraries(test_color PRIVATE m)
add_test(NAME color_interpolation COMMAND test_color)

# 3. test_bookmark (save/load precision)
add_executable(test_bookmark test_bookmark.c ../src/engine/bookmark.c ../src/engine/ini_config.c)
target_compile_options(test_bookmark PRIVATE ${TEST_FLAGS})
target_link_libraries(test_bookmark PRIVATE m)
add_test(NAME bookmark_io COMMAND test_bookmark)

# 4. test_config (ini parser)
add_executable(test_config test_config.c ../src/engine/ini_config.c)
target_compile_options(test_config PRIVATE ${TEST_FLAGS})
target_link_libraries(test_config PRIVATE m)
add_test(NAME config_parser COMMAND test_config)

# 5. test_math_f128 (128-bit precision math)
add_executable(test_math_f128 test_math.c ../src/core/mandelbrot.c ../src/core/julia.c ../src/core/burning_ship.c)
target_compile_definitions(test_math_f128 PRIVATE USE_SIMD_F128)
target_compile_options(test_math_f128 PRIVATE ${TEST_FLAGS})
target_link_libraries(test_math_f128 PRIVATE m)
add_test(NAME math_kernels_128bit COMMAND test_math_f128)

# 6. test_tour (tour state machine logic)
add_executable(test_tour test_tour.c ../src/engine/tour.c)
target_compile_options(test_tour PRIVATE ${TEST_FLAGS})
target_link_libraries(test_tour PRIVATE m)
add_test(NAME tour_state_machine COMMAND test_tour)

# 7. test_renderer (thread pool dispatch logic)
add_executable(test_renderer test_renderer.c ../src/engine/renderer.c ../src/engine/ini_config.c ../src/core/mandelbrot.c ../src/core/julia.c ../src/core/burning_ship.c ../src/core/color.c ../src/core/fractal.c)
target_compile_options(test_renderer PRIVATE ${TEST_FLAGS} -pthread)
target_link_libraries(test_renderer PRIVATE m pthread)
add_test(NAME renderer_pool COMMAND test_renderer)

