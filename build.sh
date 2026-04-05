#!/bin/bash
# Quick build script - checks dependencies and builds the project
# Usage: ./build.sh

echo ""
echo "========================================"
echo "  Mandelbrot Set Explorer - Build"
echo "========================================"
echo ""

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
else
    OS="Unknown (try make directly)"
fi
echo "OS: $OS"

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo ""
    echo "ERROR: cmake not found!"
    if [[ "$OS" == "Linux" ]]; then
        echo "Install with: sudo apt install cmake build-essential"
    elif [[ "$OS" == "macOS" ]]; then
        echo "Install with: brew install cmake"
    fi
    exit 1
fi

# Check for SDL2
if ! pkg-config --exists sdl2 2>/dev/null; then
    echo ""
    echo "ERROR: SDL2 not found!"
    echo ""
    if [[ "$OS" == "Linux" ]]; then
        echo "Install with:"
        echo "  sudo apt install libsdl2-dev libsdl2-ttf-dev   (Ubuntu/Debian)"
        echo "  sudo pacman -S sdl2 sdl2_ttf                   (Arch)"
        echo "  sudo dnf install SDL2-devel SDL2_ttf-devel     (Fedora)"
    elif [[ "$OS" == "macOS" ]]; then
        echo "Install with: brew install sdl2 sdl2_ttf"
    fi
    exit 1
fi

echo "SDL2: $(pkg-config --modversion sdl2) ✓"
echo ""

# Build!
echo "Building..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "  Build successful!"
    echo "========================================"
    echo ""
    echo "Run with: ./build/mandelbrot"
    echo ""
else
    echo ""
    echo "Build failed."
    echo "Check the errors above."
    exit 1
fi
