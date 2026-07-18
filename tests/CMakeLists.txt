# tests/CMakeLists.txt - Build configuration for unit tests

enable_testing()

# inherit compiler flags from parent (ARCH_FLAGS)
set(TEST_FLAGS "${ARCH_FLAGS}")

# 1. test_math (core math kernels, including 128-bit via global USE_SIMD_F128)
add_executable(test_math test_math.c)
target_compile_options(test_math PRIVATE ${TEST_FLAGS})
target_link_libraries(test_math PRIVATE mandelbrot_lib m)
add_test(NAME math_kernels COMMAND test_math)

# 2. test_color (palette logic)
add_executable(test_color test_color.c)
target_compile_options(test_color PRIVATE ${TEST_FLAGS})
target_link_libraries(test_color PRIVATE mandelbrot_lib m)
add_test(NAME color_interpolation COMMAND test_color)

# 3. test_bookmark (save/load precision)
add_executable(test_bookmark test_bookmark.c)
target_compile_options(test_bookmark PRIVATE ${TEST_FLAGS})
target_link_libraries(test_bookmark PRIVATE mandelbrot_lib m)
add_test(NAME bookmark_io COMMAND test_bookmark)

# 4. test_config (ini parser)
add_executable(test_config test_config.c)
target_compile_options(test_config PRIVATE ${TEST_FLAGS})
target_link_libraries(test_config PRIVATE mandelbrot_lib m)
add_test(NAME config_parser COMMAND test_config)

# 5. test_tour (tour state machine logic)
add_executable(test_tour test_tour.c)
target_compile_options(test_tour PRIVATE ${TEST_FLAGS})
target_link_libraries(test_tour PRIVATE mandelbrot_lib m)
add_test(NAME tour_state_machine COMMAND test_tour)

# 6. test_renderer (thread pool dispatch logic)
add_executable(test_renderer test_renderer.c)
target_compile_options(test_renderer PRIVATE ${TEST_FLAGS} -pthread)
target_link_libraries(test_renderer PRIVATE mandelbrot_lib m pthread)
add_test(NAME renderer_pool COMMAND test_renderer)

# 7. test_app_state (common state transitions)
add_executable(test_app_state test_app_state.c)
target_compile_options(test_app_state PRIVATE ${TEST_FLAGS} -pthread)
target_link_libraries(test_app_state PRIVATE mandelbrot_lib m pthread)
add_test(NAME app_state_manager COMMAND test_app_state)

# 8. test_perturbation (perturbation theory reference orbit logic)
add_executable(test_perturbation test_perturbation.c)
target_compile_options(test_perturbation PRIVATE ${TEST_FLAGS})
target_link_libraries(test_perturbation PRIVATE mandelbrot_lib m)
add_test(NAME perturbation_orbit COMMAND test_perturbation)

# 9. test_bignum (arbitrary-precision BigNum engine + orbit integration)
add_executable(test_bignum test_bignum.c)
target_compile_options(test_bignum PRIVATE ${TEST_FLAGS})
target_link_libraries(test_bignum PRIVATE mandelbrot_lib m)
add_test(NAME bignum_arithmetic COMMAND test_bignum)

# 10. test_fractal (other fractal equations)
add_executable(test_fractal test_fractal.c)
target_compile_options(test_fractal PRIVATE ${TEST_FLAGS})
target_link_libraries(test_fractal PRIVATE mandelbrot_lib m)
add_test(NAME fractal_equations COMMAND test_fractal)

# 11. test_camera (viewport and coordinate logic)
add_executable(test_camera test_camera.c)
target_compile_options(test_camera PRIVATE ${TEST_FLAGS})
target_link_libraries(test_camera PRIVATE mandelbrot_lib m)
add_test(NAME camera_logic COMMAND test_camera)

# 12. test_input_handler (mouse/keyboard handling)
add_executable(test_input_handler test_input_handler.c)
target_compile_options(test_input_handler PRIVATE ${TEST_FLAGS})
target_link_libraries(test_input_handler PRIVATE mandelbrot_lib m)
add_test(NAME input_handling COMMAND test_input_handler)

add_executable(test_simd_math test_simd_math.c)
target_compile_options(test_simd_math PRIVATE ${TEST_FLAGS})
target_link_libraries(test_simd_math PRIVATE mandelbrot_lib m)
add_test(NAME test_simd_math COMMAND test_simd_math)
