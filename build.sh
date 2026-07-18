#!/bin/bash

# build.sh - build script for mandelbrot engine
# optimized with parallel builds and consistent naming

build_cpu() {
    ./scripts/compile_shaders.sh || return $?
    cmake -S . -B build_cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release || return $?
    cmake --build build_cpu --parallel || return $?
    echo ""
    echo "===================================================================================="
    echo " build complete! to run cpu engine:"
    echo "  * ./build_cpu/mandelbrot_cpu"
    echo "===================================================================================="
    echo " "
}

build_gpu() {
    ./scripts/compile_shaders.sh || return $?
    cmake -S . -B build_gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release || return $?
    cmake --build build_gpu --parallel || return $?
    echo ""
    echo "===================================================================================="
    echo " build complete! to run gpu engine:"
    echo "  * ./build_gpu/mandelbrot_gpu"
    echo "===================================================================================="
    echo ""
}

build_web() {
    ./scripts/compile_shaders.sh || return $?
    if ! command -v emcmake &> /dev/null; then
        echo "error: emscripten not found. please install emsdk."
        return 1
    fi
    emcmake cmake -S . -B build_web -DBUILD_WEB=ON || return $?
    cmake --build build_web --parallel || return $?
    mkdir -p deploy
    cp web/index.html deploy/
    cp web/style.css deploy/
    cp web/app.js deploy/
    cp web/coi-serviceworker.js deploy/
    cp build_web/index.js deploy/
    cp build_web/index.wasm deploy/
    if [ -f "build_web/index.worker.js" ]; then cp build_web/index.worker.js deploy/; fi
    if [ -d "assets" ]; then cp -r assets deploy/; fi
    if [ -d "web/assets" ]; then cp -r web/assets/* deploy/assets/; fi
    echo "web: build and deployment package ready in 'deploy/' folder."
    echo ""
    echo "===================================================================================="
    echo " build complete! to run web engine:"
    echo "  * python3 scripts/server.py --dir deploy --port 8080"
    echo "  * then open http://localhost:8080"
    echo "===================================================================================="
    echo ""
}

build_deep() {
    ./scripts/compile_shaders.sh || return $?
    cmake -S . -B build_deep -DBUILD_DEEP=ON -DCMAKE_BUILD_TYPE=Release || return $?
    cmake --build build_deep --parallel || return $?
    echo ""
    echo "===================================================================================="
    echo " build complete! to run perturbation engine:"
    echo "  * ./build_deep/mandelbrot_deep"
    echo "===================================================================================="
    echo " "
}

build_video() {
    ./scripts/compile_shaders.sh || return $?
    cmake -S . -B build_video -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release || return $?
    cmake --build build_video --target mandelbrot_video --parallel || return $?
    echo ""
    echo "===================================================================================="
    echo " build complete! to run video renderer engine:"
    echo "  * ./build_video/mandelbrot_video"
    echo "===================================================================================="
    echo " "
}

run_tests() {
    ./scripts/compile_shaders.sh || return $?
    cmake -S . -B build_test -DBUILD_CPU=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release || return $?
    cmake --build build_test --parallel || return $?
    echo ""
    echo "Running tests..."
    ctest --test-dir build_test --output-on-failure
    local test_status=$?
    echo "===================================================================================="
    echo " tests complete!"
    echo "===================================================================================="
    echo " "
    return $test_status
}

run_benchmarks() {
    ./scripts/compile_shaders.sh || return $?
    cmake -S . -B build_bench -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release || return $?
    cmake --build build_bench --parallel || return $?
    echo ""
    echo "Running math benchmark..."
    ./build_bench/benchmarks/benchmark_math || return $?
    echo ""
    echo "Running perturbation benchmark..."
    ./build_bench/benchmarks/benchmark_perturbation || return $?
    echo ""
    echo "Running bignum benchmark..."
    ./build_bench/benchmarks/benchmark_bignum || return $?
    echo ""
    echo "Running renderer benchmark..."
    ./build_bench/benchmarks/benchmark_renderer || return $?
    echo ""
    echo "===================================================================================="
    echo " benchmarks complete!"
    echo "===================================================================================="
    echo " "
}

build_all() {
    build_cpu && build_gpu && build_web && build_deep && build_video
}

clean() {
    rm -rf build_cpu build_gpu build_web build_deep build_video build_test build_bench build deploy
    echo "clean complete!"
}

if [ $# -gt 0 ]; then
    case "$1" in
        cpu)   build_cpu ;;
        gpu)   build_gpu ;;
        web)   build_web ;;
        deep)  build_deep ;;
        video) build_video ;;
        test)  run_tests ;;
        bench) run_benchmarks ;;
        all)   build_all ;;
        clean) clean ;;
        *)
            echo "error: unknown option '$1'. usage: $0 {cpu|gpu|web|deep|video|test|bench|all|clean}"
            exit 1
            ;;
    esac
    exit $?
fi

echo "===================================================================================="
echo "mandelbrot engine build!!"
echo "===================================================================================="
echo "  1) cpu (combined 64/128-bit)"
echo "  2) gpu (combined 32/64-bit)"
echo "  3) web"
echo "  4) deep/infinite zoom (gpu + perturbation + bignum)"
echo "  5) video renderer"
echo "  6) run tests"
echo "  7) run benchmarks"
echo "  8) build all"
echo "  9) clean"
echo "  q) quit"
echo "===================================================================================="
echo " "
read -p ">> " choice

case $choice in
    1) build_cpu ;;
    2) build_gpu ;;
    3) build_web ;;
    4) build_deep ;;
    5) build_video ;;
    6) run_tests ;;
    7) run_benchmarks ;;
    8) build_all ;;
    9) clean ;;
    q|Q) exit 0 ;;
    *)
        echo "error: invalid choice '$choice'. please enter a number between 1-9, or 'q' to quit."
        exit 1
        ;;
esac
