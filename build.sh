#!/bin/bash

# professional build script for mandelbrot-c
# supports both interactive menu and command line arguments

build_cpu() {
    echo "building cpu-engine..."
    cmake -S . -B build-cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build-cpu
    echo "done. run: ./build-cpu/mandelbrot-cpu"
}

build_gpu() {
    echo "building gpu-engine..."
    cmake -S . -B build-gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build-gpu
    echo "done. run: ./build-gpu/mandelbrot-gpu"
}

build_web() {
    echo "building web-engine..."
    if ! command -v emcmake &> /dev/null; then
        echo "error: emscripten not found. please install emsdk."
        exit 1
    fi
    emcmake cmake -S . -B build-web -DBUILD_WEB=ON
    cmake --build build-web
    echo "done. serving from build-web..."
    echo "skipping server.py"
}

clean() {
    echo "cleaning..."
    rm -rf build-cpu build-gpu build-web build
    echo "cleaned."
}

# check for command line arguments
if [ $# -gt 0 ]; then
    case "$1" in
        cpu)   build_cpu ;;
        gpu)   build_gpu ;;
        web)   build_web ;;
        clean) clean ;;
        *)     echo "usage: $0 {cpu|gpu|web|clean}"; exit 1 ;;
    esac
    exit 0
fi

# interactive menu (if no arguments provided)
echo "========================================"
echo "    mandelbrot engine build menu"
echo "========================================"
echo "1) build cpu-engine (desktop/accurate)"
echo "2) build gpu-engine (desktop/fast)"
echo "3) build web-engine (wasm/sokol)"
echo "4) clean build artifacts"
echo "q) quit"
echo "----------------------------------------"
read -p "select option [1-4]: " choice

case $choice in
    1) build_cpu ;;
    2) build_gpu ;;
    3) build_web ;;
    4) clean ;;
    q) exit 0 ;;
    *) echo "invalid option."; exit 1 ;;
esac
