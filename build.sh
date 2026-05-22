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
    build_cpu && build_gpu && build_web
}

clean() {
    rm -rf build_cpu build_gpu build_web build deploy
    echo "clean complete!"
}

if [ $# -gt 0 ]; then
    case "$1" in
        cpu)   build_cpu ;;
        gpu)   build_gpu ;;
        web)   build_web ;;
        all)   build_all ;;
        clean) clean ;;
        *)     echo "error: unknown option '$1'. usage: $0 {cpu|gpu|web|all|clean}" ;;
    esac
    exit 0
fi

echo "===================================================================================="
echo "mandelbrot engine build!!"
echo "===================================================================================="
echo "  1) cpu (combined 64/128-bit)"
echo "  2) gpu (combined 32/64-bit)"
echo "  3) web"
echo "  4) build all"
echo "  5) clean"
echo "  q) quit"
echo "===================================================================================="
echo " "
read -p ">> " choice

case $choice in
    1) build_cpu ;;
    2) build_gpu ;;
    3) build_web ;;
    4) build_all ;;
    5) clean ;;
    q|Q) exit 0 ;;
    *)
        echo "error: invalid choice '$choice'. please enter a number between 1-5, or 'q' to quit."
        exit 1
        ;;
esac
