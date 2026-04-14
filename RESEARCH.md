# Research and Future Directions

This document outlines architectural decisions and research topics for features that are currently outside the primary scope of the main CPU-optimized engine. These features are being explored in dedicated branches or as separate research initiatives to maintain the cleanliness and performance of the core codebase.

## Arbitrary-Precision Arithmetic

### Integration of GNU MPFR
While standard `double` precision is sufficient for most fractal exploration, it reaches its limits at zoom levels approximately below 1e-15. Integrating libraries like GNU MPFR would allow for nearly infinite zoom levels.

**Status:** Researching Hybrid-Engine Approach.
**Decision Rationale:** 
- A global transition to fixed-precision types would deprecate hardware acceleration and SIMD optimizations, leading to a significant performance regression (approximately 100x slower).
- The current strategy is to develop a separate high-precision backend that is only engaged when the zoom level exceeds the hardware precision threshold.

## Advanced Backends

### GPU Acceleration (OpenGL / Vulkan)
The Mandelbrot set calculation is embarrassingly parallel and highly suited for GPGPU execution.

**Status:** Planned for a dedicated `feature/gpu` branch.
**Decision Rationale:** 
- GPU backends introduce significant binary dependencies and driver-specific overhead (Vulkan/OpenCL/CUDA).
- The primary goal of the master branch is to serve as a high-performance, dependency-light CPU reference implementation using pure C and POSIX threads.
- Bridging the current CPU architecture with a GPU pipeline requires a complete redesign of the memory dispatch system.

### WebAssembly (Emscripten)
The explorer has been successfully ported to WebAssembly, bringing high-performance fractal rendering to any modern browser.

**Status:** [IMPLEMENTED] Integrated into `master` branch.
**Decision Rationale & Technical Implementation:** 
- **Threading Model:** Emscripten's `pthreads` implementation is used to spawn a thread pool matching the user's hardware concurrency (`navigator.hardwareConcurrency`).
- **SIMD Acceleration:** The engine utilizes **WASM SIMD128** intrinsics, providing a 2x-3x speedup over scalar WASM while maintaining portability.
- **Security Headers (COOP/COEP):** Modern browsers require `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` to enable `SharedArrayBuffer` (necessary for pthreads).
- **Static Hosting Workaround:** Since GitHub Pages does not serve these headers, we implemented **`coi-serviceworker.js`**, a Service Worker shim that intercepts network requests to inject the required security headers at run-time.
