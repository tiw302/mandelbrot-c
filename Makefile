# Master Build System for Mandelbrot-C (Engine-Centric)

CFLAGS = -O3 -Wall -Wextra -Iinclude -Icore -Icpu-engine -mavx2 -mfma -pthread
LDFLAGS = -lSDL2 -lSDL2_ttf -lm -lz -lpng

WASM_FLAGS = -O3 -Wall -Wextra -Iinclude -Icore -Iweb-engine -msimd128 -D__wasm_simd128__ --use-port=sdl2 --use-port=sdl2_ttf

NVCC_FLAGS = -O3 -Iinclude -Icore -Igpu-engine

CORE_SRC = core/mandelbrot.c core/julia.c core/color.c
CPU_SRC = cpu-engine/main.c cpu-engine/renderer.c cpu-engine/screenshot.c cpu-engine/tour.c
WASM_SRC = web-engine/main_wasm.c web-engine/renderer_wasm.c
GPU_SRC = gpu-engine/mandelbrot_kernel.cu

all: cpu

cpu:
	gcc $(CFLAGS) $(CORE_SRC) $(CPU_SRC) -o mandelbrot-desktop $(LDFLAGS)

web:
	emcc $(WASM_FLAGS) $(CORE_SRC) cpu-engine/renderer.c $(WASM_SRC) \
		-s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MAX_WEBGL_VERSION=2 \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
		-pthread -s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency \
		-o mandelbrot.js

gpu:
	nvcc $(NVCC_FLAGS) $(CORE_SRC) $(GPU_SRC) -o mandelbrot-gpu

# Note: CMakeLists.txt has been fully replaced by this Engine-Centric Makefile configuration.

clean:
	rm -f mandelbrot-desktop mandelbrot.js mandelbrot.wasm mandelbrot-gpu
