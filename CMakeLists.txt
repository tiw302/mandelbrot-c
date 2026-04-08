cmake_minimum_required(VERSION 3.10)
project(mandelbrot VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB SOURCES "src/*.c")
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

# Performance optimizations
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O3 -march=native -ffast-math)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /O2 /fp:fast /arch:AVX2)
endif()

# Build dependencies
find_package(ZLIB REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread)
endif()

# SDL2 platform-specific loading
if(WIN32 AND NOT MSYS AND NOT MINGW)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
else()
    # UNIX, MSYS, MinGW, macOS
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_ttf)
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SDL2)
endif()

# enable ctest and add tests folder
enable_testing()
add_subdirectory(tests)

