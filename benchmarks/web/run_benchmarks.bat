@echo off
REM run_benchmarks.bat
REM 
REM compiles and runs cpu benchmarks using emscripten to measure webassembly overhead.
REM requires emcc and node to be installed and in PATH.

REM verify emcc is available
where emcc >nul 2>nul
if %errorlevel% neq 0 (
    echo error: emcc (emscripten) not found in PATH.
    echo please run 'emsdk_env.bat' in your emscripten directory.
    exit /b 1
)

REM verify node is available
where node >nul 2>nul
if %errorlevel% neq 0 (
    echo error: node.js not found in PATH.
    exit /b 1
)

echo ==========================================
echo  WebAssembly Performance Benchmarks       
echo ==========================================

set BUILD_DIR=build_wasm_bench
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM common include paths
set INCLUDES=-I..\..\include -I..\..\include\core -I..\..\include\engine
REM compile the math library
set MATH_SRCS=..\..\src\core\mandelbrot.c ..\..\src\core\burning_ship.c ..\..\src\core\tricorn.c ..\..\src\core\celtic.c ..\..\src\core\buffalo.c ..\..\src\core\mandelbrot_bignum.c ..\..\src\core\julia.c ..\..\src\core\fractal.c

echo [1/2] Compiling benchmark_math for WebAssembly...
call emcc ..\cpu\benchmark_math.c %MATH_SRCS% %INCLUDES% -O3 -s WASM=1 -s NODERAWFS=1 -o "%BUILD_DIR%\benchmark_math.js"

echo [2/2] Compiling benchmark_fractal_types for WebAssembly...
call emcc ..\cpu\benchmark_fractal_types.c %MATH_SRCS% %INCLUDES% -O3 -s WASM=1 -s NODERAWFS=1 -o "%BUILD_DIR%\benchmark_fractal_types.js"

echo.
echo ^>^>^> RUNNING benchmark_math (WebAssembly/Node.js) ^<^<^<
node "%BUILD_DIR%\benchmark_math.js"

echo.
echo ^>^>^> RUNNING benchmark_fractal_types (WebAssembly/Node.js) ^<^<^<
node "%BUILD_DIR%\benchmark_fractal_types.js"

echo ==========================================
echo  WebAssembly Benchmarks Complete          
echo ==========================================
