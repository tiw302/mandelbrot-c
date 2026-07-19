@echo off
setlocal EnableDelayedExpansion

set DIR=%~dp0
set PROJECT_DIR=%DIR%..
set BIN_DIR=%PROJECT_DIR%\bin
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

set SHDC_BIN=%BIN_DIR%\sokol-shdc.exe

if not exist "%SHDC_BIN%" (
    echo Downloading sokol-shdc.exe...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/floooh/sokol-tools-bin/raw/master/bin/win32/sokol-shdc.exe' -OutFile '%SHDC_BIN%'"
)

echo Compiling shaders...
"%SHDC_BIN%" -i "%PROJECT_DIR%\shaders\mandelbrot.glsl" -o "%PROJECT_DIR%\src\app\shaders.h" -l glsl410:glsl300es:wgsl -f sokol
echo Shaders compiled successfully.
