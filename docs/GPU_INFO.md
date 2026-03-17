# Why no GPU Acceleration?

You might be wondering: *"Why is this Mandelbrot renderer using the CPU when GPUs are thousands of times faster for parallel tasks?"*

Great question! ( •⌄• )✧

This project was created primarily as a learning exercise to understand:
1.  **Low-level threading:** Manually managing `pthread` creation, synchronization, and workload distribution.
2.  **Memory management:** Understanding how to optimize memory access patterns for CPU caches.
3.  **SIMD instructions:** Exploring how vectorization (AVX/SSE) works on modern processors.

While a GPU shader (GLSL/CUDA) would be objectively faster, implementing this on the CPU taught me valuable lessons about concurrency and system architecture.

### Future Plans?

Maybe! I am considering adding a GPU backend (using OpenGL or Vulkan) in the future to compare performance. But for now, I am focusing on squeezing every last drop of performance out of the CPU. (ง •̀_•́)ง
