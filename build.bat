@echo off
setlocal EnableDelayedExpansion

:: -----------------------------------------------------------------------------
:: build.bat - Windows Build script for Mandelbrot engine
:: Optimized with parallel builds and consistent naming
:: -----------------------------------------------------------------------------

:: Check if command line arguments are provided
if not "%~1"=="" (
    if /i "%~1"=="cpu" goto build_cpu
    if /i "%~1"=="gpu" goto build_gpu
    if /i "%~1"=="web" goto build_web
    if /i "%~1"=="deep" goto build_deep
    if /i "%~1"=="video" goto build_video
    if /i "%~1"=="test" goto run_tests
    if /i "%~1"=="bench" goto run_benchmarks
    if /i "%~1"=="all" goto build_all
    if /i "%~1"=="clean" goto clean
    echo error: unknown option '%~1'. usage: %0 {cpu^|gpu^|web^|deep^|video^|test^|bench^|all^|clean}
    exit /b 1
)

:menu
echo ====================================================================================
echo mandelbrot engine build!! (Windows)
echo ====================================================================================
echo   1^) cpu (combined 64/128-bit)
echo   2^) gpu (combined 32/64-bit)
echo   3^) web
echo   4^) deep zoom (gpu + perturbation)
echo   5^) video renderer (headless)
echo   6^) run tests
echo   7^) run benchmarks
echo   8^) build all
echo   9^) clean
echo   q^) quit
echo ====================================================================================
echo.
set /p choice=">> "

if /i "%choice%"=="1" goto build_cpu
if /i "%choice%"=="2" goto build_gpu
if /i "%choice%"=="3" goto build_web
if /i "%choice%"=="4" goto build_deep
if /i "%choice%"=="5" goto build_video
if /i "%choice%"=="6" goto run_tests
if /i "%choice%"=="7" goto run_benchmarks
if /i "%choice%"=="8" goto build_all
if /i "%choice%"=="9" goto clean
if /i "%choice%"=="q" exit /b 0

echo error: invalid choice '%choice%'. please enter a number between 1-9, or 'q' to quit.
echo.
goto menu

:build_cpu
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Configuring CPU build...
cmake -S . -B build_cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building CPU engine...
cmake --build build_cpu --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo ====================================================================================
echo  build complete! to run cpu engine:
echo   * .\build_cpu\Release\mandelbrot_cpu.exe
echo ====================================================================================
echo.
goto end

:build_gpu
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Configuring GPU build...
cmake -S . -B build_gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building GPU engine...
cmake --build build_gpu --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo ====================================================================================
echo  build complete! to run gpu engine:
echo   * .\build_gpu\Release\mandelbrot_gpu.exe
echo ====================================================================================
echo.
goto end

:build_web
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
where emcmake >nul 2>nul
if %errorlevel% neq 0 (
    echo error: emscripten not found. please install emsdk and run emsdk_env.bat.
    exit /b 1
)
echo Configuring Web build...
call emcmake cmake -S . -B build_web -DBUILD_WEB=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building Web engine...
cmake --build build_web --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%

echo Packaging for deployment...
if not exist deploy mkdir deploy
copy /y web\index.html deploy\ >nul
copy /y web\style.css deploy\ >nul
copy /y web\app.js deploy\ >nul
copy /y web\coi-serviceworker.js deploy\ >nul
copy /y build_web\index.js deploy\ >nul
copy /y build_web\index.wasm deploy\ >nul
if exist build_web\index.worker.js copy /y build_web\index.worker.js deploy\ >nul
if exist assets\ (
    xcopy /s /y /i assets deploy\assets\ >nul
)
if exist web\assets\ (
    xcopy /s /y /i web\assets\* deploy\assets\ >nul
)
echo web: build and deployment package ready in 'deploy\' folder.
echo.
echo ====================================================================================
echo  build complete! to run web engine:
echo   * python scripts\server.py --dir deploy --port 8080
echo   * then open http://localhost:8080
echo ====================================================================================
echo.
goto end

:build_deep
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Configuring Deep Zoom (Perturbation) build...
cmake -S . -B build_deep -DBUILD_DEEP=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building Perturbation engine...
cmake --build build_deep --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo ====================================================================================
echo  build complete! to run perturbation engine:
echo   * .\build_deep\Release\mandelbrot_deep.exe
echo ====================================================================================
echo.
goto end

:build_video
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Configuring Video Renderer build...
cmake -S . -B build_video -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building Video Renderer...
cmake --build build_video --target mandelbrot_video --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo ====================================================================================
echo  build complete! to run video renderer engine:
echo   * .\build_video\Release\mandelbrot_video.exe
echo ====================================================================================
echo.
goto end

:run_tests
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Configuring tests...
cmake -S . -B build_test -DBUILD_CPU=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building tests...
cmake --build build_test --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Running tests...
ctest --test-dir build_test -C Release --output-on-failure
set BUILD_EXIT_CODE=%errorlevel%
echo ====================================================================================
echo  tests complete!
echo ====================================================================================
echo.
goto end

:run_benchmarks
call scripts\compile_shaders.bat
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Configuring benchmarks...
cmake -S . -B build_bench -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo Building benchmarks...
cmake --build build_bench --parallel --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
echo.
echo Running math benchmark...
if exist build_bench\benchmarks\Release\benchmark_math.exe (
    build_bench\benchmarks\Release\benchmark_math.exe
) else (
    build_bench\benchmarks\benchmark_math.exe
)
echo.
echo Running perturbation benchmark...
if exist build_bench\benchmarks\Release\benchmark_perturbation.exe (
    build_bench\benchmarks\Release\benchmark_perturbation.exe
) else (
    build_bench\benchmarks\benchmark_perturbation.exe
)
echo.
echo Running bignum benchmark...
if exist build_bench\benchmarks\Release\benchmark_bignum.exe (
    build_bench\benchmarks\Release\benchmark_bignum.exe
) else (
    build_bench\benchmarks\benchmark_bignum.exe
)
echo.
echo Running renderer benchmark...
if exist build_bench\benchmarks\Release\benchmark_renderer.exe (
    build_bench\benchmarks\Release\benchmark_renderer.exe
) else (
    build_bench\benchmarks\benchmark_renderer.exe
)
echo.
echo ====================================================================================
echo  benchmarks complete!
echo ====================================================================================
echo.
goto end

:build_all
call :build_cpu
call :build_gpu
call :build_web
call :build_deep
call :build_video
goto end

:clean
echo.
echo Cleaning build directories...
if exist build_cpu rmdir /s /q build_cpu
if exist build_gpu rmdir /s /q build_gpu
if exist build_web rmdir /s /q build_web
if exist build_deep rmdir /s /q build_deep
if exist build_video rmdir /s /q build_video
if exist build_test rmdir /s /q build_test
if exist build_bench rmdir /s /q build_bench
if exist build rmdir /s /q build
if exist deploy rmdir /s /q deploy
echo clean complete!
echo.
goto end

:end
if "%~1"=="" goto menu
if "%BUILD_EXIT_CODE%"=="" set BUILD_EXIT_CODE=0
exit /b %BUILD_EXIT_CODE%
