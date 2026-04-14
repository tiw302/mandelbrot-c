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
if(EMSCRIPTEN)
    # Emscripten uses its own SDL2 ports
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    
    # Link against Emscripten's SDL2/SDL2_ttf ports
    # -pthread is required for multi-threading in Wasm
    target_link_options(${PROJECT_NAME} PRIVATE 
        "-s USE_SDL=2"
        "-s USE_SDL_TTF=2"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s EXIT_RUNTIME=1"
        "-s INITIAL_MEMORY=64MB"
        "-pthread"
        "--preload-file assets"  # If we have assets, preload them
        "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    )
    target_compile_options(${PROJECT_NAME} PRIVATE "-s USE_SDL=2" "-s USE_SDL_TTF=2" "-pthread")
elseif(WIN32 AND NOT MSYS AND NOT MINGW)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
else()
    # UNIX, MSYS, MinGW, macOS
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_ttf)
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SDL2)
endif()

# ── Unit tests ────────────────────────────────────────────────────────────────
# Skip tests for Wasm for now, or build them differently
if(NOT EMSCRIPTEN)
    enable_testing()
    add_executable(test_math
        tests/test_math.c
        src/mandelbrot.c
        src/julia.c
    )
    target_include_directories(test_math PRIVATE include tests)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_math PRIVATE -Wall -Wextra -O2)
    elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
        target_compile_options(test_math PRIVATE /W4 /O1)
    endif()

    if(UNIX)
        target_link_libraries(test_math PRIVATE m)
    endif()

    add_test(NAME math_correctness COMMAND test_math)
endif()
