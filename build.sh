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
        # Note: $ID_LIKE can handle derivatives like Linux Mint ("ubuntu") or Manjaro ("arch")
        # We check $ID_LIKE first, fallback to $ID
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
else
    OS="Unknown"
fi

echo "OS: $OS"

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo ""
    echo "ERROR: cmake not found!"
    if [[ "$OS" == "Linux" || "$OS" == "macOS" ]]; then
        echo "Install dependencies with: $PM_CMD $PKG_CMAKE"
    fi
    exit 1
fi

# Check for pkg-config
if ! command -v pkg-config &> /dev/null; then
    echo ""
    echo "ERROR: pkg-config not found!"
    if [[ "$OS" == "Linux" || "$OS" == "macOS" ]]; then
        echo "Install dependencies with: $PM_CMD $PKG_CMAKE"
    fi
    exit 1
fi

# Check for SDL2 and SDL2_ttf
if ! pkg-config --exists sdl2 sdl2_ttf 2>/dev/null; then
    echo ""
    echo "ERROR: SDL2 or SDL2_ttf not found!"
    echo ""
    if [[ "$OS" == "Linux" || "$OS" == "macOS" ]]; then
        echo "Install libraries with: $PM_CMD $PKG_SDL"
    fi
    exit 1
fi

echo "SDL2: $(pkg-config --modversion sdl2) OK"
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
