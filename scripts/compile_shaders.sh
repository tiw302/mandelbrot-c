#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( dirname "$DIR" )"
BIN_DIR="$PROJECT_DIR/bin"
mkdir -p "$BIN_DIR"

SHDC_BIN="$BIN_DIR/sokol-shdc"

if [ ! -f "$SHDC_BIN" ]; then
    echo "Downloading sokol-shdc..."
    OS=$(uname -s)
    ARCH=$(uname -m)

    if [ "$OS" = "Linux" ]; then
        URL="https://github.com/floooh/sokol-tools-bin/raw/master/bin/linux/sokol-shdc"
    elif [ "$OS" = "Darwin" ]; then
        if [ "$ARCH" = "arm64" ]; then
            URL="https://github.com/floooh/sokol-tools-bin/raw/master/bin/osx_arm64/sokol-shdc"
        else
            URL="https://github.com/floooh/sokol-tools-bin/raw/master/bin/osx/sokol-shdc"
        fi
    else
        echo "Unsupported OS for automatic sokol-shdc download"
        exit 1
    fi

    wget -qO "$SHDC_BIN" "$URL"
    chmod +x "$SHDC_BIN"
fi

echo "Compiling shaders..."
"$SHDC_BIN" -i "$PROJECT_DIR/shaders/mandelbrot.glsl" -o "$PROJECT_DIR/src/app/shaders.h" -l glsl410:glsl300es:wgsl -f sokol
echo "Shaders compiled successfully."
