#!/bin/bash

# build.sh - Build script for Mandelbrot engine
# Optimized with parallel builds and consistent naming

build_cpu() {
    cmake -S . -B build_cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build_cpu --parallel
    echo ""
    echo "===================================================================================="
    echo " build complete! to run cpu engine:"
    echo "  * ./build_cpu/mandelbrot_cpu"
    echo "===================================================================================="
    echo " "
}

build_cpu_128() {
    cmake -S . -B build_cpu_128 -DBUILD_CPU_128=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build_cpu_128 --parallel
    echo ""
    echo "===================================================================================="
    echo " build complete! to run cpu 128-bit engine:"
    echo "  * ./build_cpu_128/mandelbrot_cpu_128"
    echo "===================================================================================="
    echo " "
}

build_gpu() {
    cmake -S . -B build_gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build_gpu --parallel
    echo ""
    echo "===================================================================================="
    echo " build complete! to run gpu engine:"
    echo "  * ./build_gpu/mandelbrot_gpu"
    echo "===================================================================================="
    echo ""
}

build_web() {
    if ! command -v emcmake &> /dev/null; then
        echo "error: emscripten not found. please install emsdk."
        exit 1
    fi
    emcmake cmake -S . -B build_web -DBUILD_WEB=ON
    cmake --build build_web --parallel
    mkdir -p deploy
    cp web/index.html deploy/
    cp web/style.css deploy/
    cp web/app.js deploy/
    cp web/coi-serviceworker.js deploy/
    cp build_web/index.js deploy/
    cp build_web/index.wasm deploy/
    if [ -d "assets" ]; then cp -r assets deploy/; fi
    echo "web: build and deployment package ready in 'deploy/' folder."
    echo ""
    echo "===================================================================================="
    echo " build complete! to run web engine:"
    echo "  * python3 scripts/server.py --dir deploy --port 8080"
    echo "  * then open http://localhost:8080"
    echo "===================================================================================="
    echo ""
}

build_all() {
    build_cpu && build_cpu_128 && build_gpu && build_web
}

clean() {
    rm -rf build_cpu build_cpu_128 build_gpu build_web build deploy
}

if [ $# -gt 0 ]; then
    case "$1" in
        cpu)     build_cpu ;;
        cpu128)  build_cpu_128 ;;
        gpu)   build_gpu ;;
        web)   build_web ;;
        all)   build_all ;;
        clean) clean ;;
        *)     echo "usage: $0 {cpu|cpu128|gpu|web|all|clean}" ;;
    esac
    exit 0
fi

echo " "
echo "===================================================================================="
echo "mandelbrot engine build!!"
echo "===================================================================================="
echo "  1) cpu (64-bit)"
echo "  2) cpu (128-bit)"
echo "  3) gpu"
echo "  4) web"
echo "  5) build all"
echo "  6) clean"
echo "  q) quit"
echo "===================================================================================="
echo " "
read -p ">> " choice

case $choice in
    1) build_cpu ;;
    2) build_cpu_128 ;;
    3) build_gpu ;;
    4) build_web ;;
    5) build_all ;;
    6) clean ;;
    q) exit 0 ;;
esac
