# test runner for core math functions
add_executable(test_math test_math.c)

# we need to link against the math logic
target_link_libraries(test_math PRIVATE m)

# includes from the main project
target_include_directories(test_math PRIVATE ../include)

# link the actual sources needed for the test
# note: we point to the source files directly to avoid complex lib setups for now
target_sources(test_math PRIVATE 
    ../src/mandelbrot.c 
    ../src/julia.c
)

# same compile options as the main app for consistency
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_math PRIVATE -Wall -Wextra -O3 -march=native -ffast-math)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_compile_options(test_math PRIVATE /W4 /O2 /fp:fast /arch:AVX2)
endif()

# easy run command
add_test(NAME math_tests COMMAND test_math)
