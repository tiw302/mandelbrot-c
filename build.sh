#!/bin/bash
# Quick build script - checks dependencies and builds the project
# Usage: ./build.sh

echo ""
echo "========================================"
echo "  Mandelbrot Set Explorer - Build"
echo "========================================"
echo ""

# Detect OS and package manager
PM_CMD=""
PKG_CMAKE=""
PKG_SDL=""

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
    if [[ -f /etc/os-release ]]; then
        source /etc/os-release
        DISTRO="${ID_LIKE:-$ID}"
        case "$DISTRO" in
            *ubuntu*|*debian*|*mint*|*pop*)
                PM_CMD="sudo apt install"
                PKG_CMAKE="cmake build-essential pkg-config"
                PKG_SDL="libsdl2-dev libsdl2-ttf-dev"
                ;;
            *fedora*|*rhel*|*centos*)
                PM_CMD="sudo dnf install"
                PKG_CMAKE="cmake gcc gcc-c++ make pkgconf-pkg-config"
                PKG_SDL="SDL2-devel SDL2_ttf-devel"
                ;;
            *arch*|*manjaro*)
                PM_CMD="sudo pacman -S"
                PKG_CMAKE="cmake base-devel pkgconf"
                PKG_SDL="sdl2 sdl2_ttf"
                ;;
            *void*)
                PM_CMD="sudo xbps-install -S"
                PKG_CMAKE="cmake base-devel pkg-config"
                PKG_SDL="SDL2-devel SDL2_ttf-devel"
                ;;
            *opensuse*|*suse*)
                PM_CMD="sudo zypper in"
                PKG_CMAKE="cmake gcc gcc-c++ make pkgconf-pkg-config"
                PKG_SDL="libSDL2-devel libSDL2_ttf-devel"
                ;;
            *)
                PM_CMD="<package-manager> install"
                PKG_CMAKE="cmake gcc make pkg-config"
                PKG_SDL="sdl2 sdl2_ttf"
                ;;
        esac
    else
        PM_CMD="<package-manager> install"
        PKG_CMAKE="cmake gcc make pkg-config"
        PKG_SDL="sdl2 sdl2_ttf"
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
    PM_CMD="brew install"
    PKG_CMAKE="cmake pkg-config"
    PKG_SDL="sdl2 sdl2_ttf"
elif [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* ]]; then
    OS="Windows (MSYS2/Git Bash)"
    PM_CMD="pacman -S"
    PKG_CMAKE="mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc"
    PKG_SDL="mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf"
else
    OS="Unknown ($OSTYPE)"
fi

echo "OS Detection: $OS"

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo ""
    echo "ERROR: cmake not found!"
    if [[ -n "$PM_CMD" ]]; then
        echo "Install dependencies with: $PM_CMD $PKG_CMAKE"
    fi
    exit 1
fi

# Check for pkg-config
if ! command -v pkg-config &> /dev/null; then
    echo ""
    echo "ERROR: pkg-config not found!"
    if [[ -n "$PM_CMD" ]]; then
        echo "Install dependencies with: $PM_CMD $PKG_CMAKE"
    fi
    exit 1
fi

# Check for SDL2 and SDL2_ttf (handling case sensitivity)
MISSING_SDL=0
if ! pkg-config --exists sdl2 2>/dev/null; then
    MISSING_SDL=1
fi

MISSING_TTF=0
if ! pkg-config --exists sdl2_ttf 2>/dev/null && ! pkg-config --exists SDL2_ttf 2>/dev/null; then
    MISSING_TTF=1
fi

if [[ $MISSING_SDL -eq 1 || $MISSING_TTF -eq 1 ]]; then
    echo ""
    echo "ERROR: Missing dependencies:"
    [[ $MISSING_SDL -eq 1 ]] && echo "  - SDL2"
    [[ $MISSING_TTF -eq 1 ]] && echo "  - SDL2_ttf"
    echo ""
    if [[ -n "$PM_CMD" ]]; then
        echo "Install libraries with: $PM_CMD $PKG_SDL"
    fi
    exit 1
fi

# Get version for display
SDL_VER=$(pkg-config --modversion sdl2)
TTF_VER=$(pkg-config --modversion sdl2_ttf 2>/dev/null || pkg-config --modversion SDL2_ttf 2>/dev/null)

echo "Dependencies: SDL2 ($SDL_VER), SDL2_ttf ($TTF_VER) OK"
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
