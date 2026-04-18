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

### Cross-Platform GPU Acceleration (Sokol GFX)
The Mandelbrot set calculation is embarrassingly parallel and highly suited for GPU execution. To overcome the limitations of platform-specific APIs (CUDA/DirectX), we have transitioned towards a unified shader-based approach.

**Status:** Transitioned to Sokol GFX architecture.
**Decision Rationale:** 
- **Portability:** Sokol GFX provides a thin wrapper over Metal, Vulkan, GL, and D3D, allowing for "write-once, run-anywhere" GPU logic.
- **Dependency Management:** By using Sokol, we maintain a lightweight footprint without requiring heavy SDKs like CUDA or Vulkan in the main branch.
- **Future Integration:** Research is ongoing to move the core fractal computation entirely into GLSL/HLSL shaders to achieve extreme-scale rendering speeds.

### WebAssembly Subsystem
The explorer has been successfully ported to WebAssembly, bringing high-performance fractal rendering to any modern browser using the Sokol-based runtime.

**Status:** [IMPLEMENTED] Integrated into `master` branch.
**Technical Implementation:** 
- **Threading Model:** Emscripten's `pthreads` implementation is used to spawn a thread pool matching the user's hardware concurrency (`navigator.hardwareConcurrency`).
- **SIMD Acceleration:** The engine utilizes **WASM SIMD128** intrinsics, providing a 2x-3x speedup over scalar WASM while maintaining portability.
- **Graphics Pipeline:** Transitioned from raw SDL2-to-WASM mapping to a specialized Sokol GFX pipeline, improving frame-rate stability and WebGL 2.0 compatibility.
- **Security Headers (COOP/COEP):** Modern browsers require `SharedArrayBuffer` for multithreading. Since GitHub Pages does not serve the required COOP/COEP headers, we utilize `coi-serviceworker.js` to inject these headers at run-time.
