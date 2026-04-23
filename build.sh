#!/bin/bash

build_cpu() {
    cmake -S . -B build-cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build-cpu
}

build_gpu() {
    cmake -S . -B build-gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build-gpu
}

build_web() {
    if ! command -v emcmake &> /dev/null; then
        echo "error: emscripten not found. please install emsdk."
        exit 1
    fi
    emcmake cmake -S . -B build-web -DBUILD_WEB=ON
    cmake --build build-web
    mkdir -p deploy
    cp web/index.html deploy/
    cp web/style.css deploy/
    cp web/app.js deploy/
    cp web/coi-serviceworker.js deploy/
    cp build-web/index.js deploy/
    cp build-web/index.wasm deploy/
    if [ -d "assets" ]; then cp -r assets deploy/; fi
    echo "web: build and deployment package ready in 'deploy/' folder."
}

build_all() {
    build_cpu && build_gpu && build_web
}

clean() {
    rm -rf build-cpu build-gpu build-web build
}

if [ $# -gt 0 ]; then
    case "$1" in
        cpu)   build_cpu ;;
        gpu)   build_gpu ;;
        web)   build_web ;;
        all)   build_all ;;
        clean) clean ;;
        *)     echo "usage: $0 {cpu|gpu|web|all|clean}" ;;
    esac
    exit 0
fi

echo " "
echo "mandelbrot engine build!!"
echo " "
echo "___________________________"
echo "  1) cpu"
echo "  2) gpu"
echo "  3) web"
echo "  4) build all"
echo "  5) clean"
echo "  q) quit"
echo "___________________________"
echo " "
read -p ">> " choice

case $choice in
    1) build_cpu ;;
    2) build_gpu ;;
    3) build_web ;;
    4) build_all ;;
    5) clean ;;
    q) exit 0 ;;
esac