#!/bin/bash
# run_benchmarks.sh
# 
# compiles and runs cpu benchmarks using emscripten to measure webassembly overhead.
# requires emcc and node to be installed and in PATH.

set -e

# verify emcc is available
if ! command -v emcc &> /dev/null; then
    echo "error: emcc (emscripten) not found in PATH."
    echo "please run 'source ./emsdk_env.sh' in your emscripten directory."
    exit 1
fi

# verify node is available
if ! command -v node &> /dev/null; then
    echo "error: node.js not found in PATH."
    exit 1
fi

echo "=========================================="
echo " WebAssembly Performance Benchmarks       "
echo "=========================================="

BUILD_DIR="build_wasm_bench"
mkdir -p "$BUILD_DIR"

# common include paths
INCLUDES="-I../../include -I../../include/core -I../../include/engine"
# compile the math library
MATH_SRCS="../../src/core/mandelbrot.c ../../src/core/burning_ship.c ../../src/core/tricorn.c ../../src/core/celtic.c ../../src/core/buffalo.c ../../src/core/mandelbrot_bignum.c ../../src/core/julia.c ../../src/core/fractal.c"

echo "[1/2] Compiling benchmark_math for WebAssembly..."
emcc ../cpu/benchmark_math.c $MATH_SRCS $INCLUDES \
    -O3 -s WASM=1 -s NODERAWFS=1 -o "$BUILD_DIR/benchmark_math.js"

echo "[2/2] Compiling benchmark_fractal_types for WebAssembly..."
emcc ../cpu/benchmark_fractal_types.c $MATH_SRCS $INCLUDES \
    -O3 -s WASM=1 -s NODERAWFS=1 -o "$BUILD_DIR/benchmark_fractal_types.js"

echo ""
echo ">>> RUNNING benchmark_math (WebAssembly/Node.js) <<<"
node "$BUILD_DIR/benchmark_math.js"

echo ""
echo ">>> RUNNING benchmark_fractal_types (WebAssembly/Node.js) <<<"
node "$BUILD_DIR/benchmark_fractal_types.js"

echo "=========================================="
echo " WebAssembly Benchmarks Complete          "
echo "=========================================="
