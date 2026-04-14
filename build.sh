#!/bin/bash
# Engine-Centric Build Script
# Usage: ./build.sh [target]
# Targets: cpu, web, gpu, all, clean

echo ""
echo "========================================"
echo "  Mandelbrot Set Explorer - Build"
echo "========================================"
echo ""

TARGET=${1:-cpu}

echo "Selected Target: $TARGET"
echo ""

# Dependency checks based on target
if [[ "$TARGET" == "cpu" || "$TARGET" == "all" ]]; then
    if ! command -v pkg-config &> /dev/null; then
        echo "ERROR: pkg-config not found! (Required for CPU build)"
        exit 1
    fi
    
    if ! pkg-config --exists sdl2 2>/dev/null || (! pkg-config --exists sdl2_ttf 2>/dev/null && ! pkg-config --exists SDL2_ttf 2>/dev/null); then
        echo "ERROR: SDL2 or SDL2_ttf not found! (Required for CPU build)"
        exit 1
    fi
    echo "CPU Dependencies OK"
elif [[ "$TARGET" == "web" || "$TARGET" == "all" ]]; then
    if ! command -v emcc &> /dev/null; then
        echo "ERROR: emcc (Emscripten) not found! (Required for Web build)"
        echo "Please activate emcc: source /path/to/emsdk_env.sh"
        exit 1
    fi
    echo "Web Dependencies OK"
elif [[ "$TARGET" == "gpu" || "$TARGET" == "all" ]]; then
    if ! command -v nvcc &> /dev/null; then
        echo "ERROR: nvcc (CUDA Toolkit) not found! (Required for GPU build)"
        exit 1
    fi
    echo "GPU Dependencies OK"
fi

echo "Building $TARGET..."
make $TARGET

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "  Build successful for: $TARGET!"
    echo "========================================"
    echo ""
    if [[ "$TARGET" == "cpu" ]]; then
        echo "Run with: ./mandelbrot-desktop"
    elif [[ "$TARGET" == "web" ]]; then
        echo "Run with your HTML server. Output generated in: mandelbrot.js / mandelbrot.wasm"
    elif [[ "$TARGET" == "gpu" ]]; then
        echo "Run with: ./mandelbrot-gpu"
    fi
    echo ""
else
    echo ""
    echo "Build failed."
    echo "Check the errors above."
    exit 1
fi
