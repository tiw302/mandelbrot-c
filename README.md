# Mandelbrot-C

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/Language-C11-00599C.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
![GitHub repo size](https://img.shields.io/github/repo-size/tiw302/mandelbrot-c)
![GitHub last commit](https://img.shields.io/github/last-commit/tiw302/mandelbrot-c)

A high-performance, multi-threaded Mandelbrot and Julia set explorer written in C.
This project uses an Engine-Centric Architecture targeting Native Desktop (CPU/AVX2), Web (WebAssembly/SIMD128), and GPU (Sokol GFX / Shaders).

Live Web Demo - 
**[(https://tiw302.github.io/mandelbrot-c/)](https://tiw302.github.io/mandelbrot-c/)**

---

## Table of Contents

- [Introduction](#introduction)
- [The Math](#the-math)
- [Features](#features)
- [Technical Architecture](#technical-architecture)
- [Quality Assurance](#quality-assurance)
- [Prerequisites](#prerequisites)
- [Build and Run](#build--run)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Introduction

Hi! I am currently diving into C programming and wanted to build something visual to understand pointers, memory, and hardware acceleration better. This project is the result of my experiments with SDL2, pthreads, SIMD intrinsics, and WebAssembly targeting.

For an in-depth technical analysis of the project's architecture, SIMD optimizations, and future development paths, please refer to the [Technical Research Document](RESEARCH.md).

### Mandelbrot

![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot.png)
![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot2.png)
![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot3.png)

### Julia

![julia Screenshot](assets/julia-Screenshot.png)
![julia Screenshot](assets/julia-Screenshot2.png)
![julia Screenshot](assets/julia-Screenshot3.png)

## The Math

The Mandelbrot set is the set of complex numbers $c$ for which the function $f_c(z) = z^2 + c$ does not diverge when iterated from $z = 0$.

In simple terms:
1. Start with $z = 0$.
2. Calculate the next value: $z_{new} = z_{old}^2 + c$.
3. Repeat. If the magnitude $|z|$ stays small forever, the point $c$ is inside the set (colored black).
4. If $|z|$ explodes (escapes to infinity), the point is outside. The color represents **how fast** it escaped.

### Julia Sets

A Julia set $J_c$ is the closely related fractal you get when you **fix** $c$ and let the starting point $z$ vary across the screen instead. Every single point inside the Mandelbrot set produces a different, connected Julia set. Points near the boundary of the Mandelbrot set produce the most intricate Julia sets -- which is exactly why the interactive mode is so fun to explore.

## Features

- **Real-time Rendering:** Optimized arithmetic for smooth navigation.
- **Hardware Acceleration:** AVX2 and WASM SIMD128 vectorization for processing multiple pixels in parallel.
- **Dynamic Multi-threading:** Intelligent row-based workload distribution using pthreads.
- **Cross-Platform Engines:** Engine-Centric monorepo structure mapping logic to CPU, Web, and GPU targets.
- **Julia Set Mode:** Press `J` to instantly switch to the Julia set defined by the point under your cursor.
- **Tour Mode:** Automated exploration paths for both Mandelbrot and Julia modes.
- **Screenshot Export:** PNG export functionality with no external library dependencies (uses integrated zlib and libpng).
- **Interactive Controls:** Advanced mouse-based panning and selection-based zooming.
- **Robustness:** Audited for memory safety and edge-case stability (stable during rapid window resizing).

## Technical Architecture

### Engine-Centric Architecture
The codebase strictly adheres to an Engine-Centric architecture to ensure Separation of Concerns (SoC).
- **Core [SSOT]:** Pure mathematical definitions (`mandelbrot.c`, `julia.c`) reside here as the Single Source of Truth. They are entirely agnostic to rendering APIs, utilizing compiler intrinsics natively.
- **CPU Engine:** Responsible for Native Desktop rendering using SDL2, handling input polling, and distributing workloads across logical core thread pools.
- **Web Engine:** Contains Emscripten logic (`main_wasm.c`) bridging the core mathematics to HTML5 distributions.
- **GPU Engine:** Houses cross-platform shader implementations using `sokol_gfx.h` for extreme high-performance parallel GPU execution across GL, Metal, and D3D.

### WebAssembly Subsystem
The WebAssembly implementation enables high-performance desktop-class rendering within the browser environment.
- **Multithreading:** Utilizes Emscripten's `pthreads` implementation by leveraging Web Workers and `SharedArrayBuffer` to parallelize the workload across all available logical CPU cores.
- **Instruction Optimization:** Implements WASM SIMD128 intrinsics, allowing for simultaneous processing of two double-precision complex numbers per instruction.
- **Security Compliance:** For deployment on static hosting platforms (e.g., GitHub Pages), the engine utilizes a specialized Service Worker (`coi-serviceworker.js`) to enforce Cross-Origin Opener Policy (COOP) and Cross-Origin Embedder Policy (COEP) headers, which are required for multithreading.
- **Input Handling:** Touch events (pinch-to-zoom, panning) are natively mapped to C logic and throttled via `requestAnimationFrame` to ensure frame-rate stability.

### Vectorized Optimization (SIMD)
The fractal engine utilizes 256-bit AVX2 registers on desktop and 128-bit SIMD on WebAssembly. This allows the system to perform complex squaring, addition, and escape radius testing on 4 (or 2) independent double-precision points simultaneously natively.

### Load Balancing
Parallelization is managed through a dynamic scheduling model on the CPU. Unlike static partitioning, which can lead to inefficient CPU usage in regions of high iteration density, the engine uses atomic counters to allow threads to claim the next available row. 

## Quality Assurance

### Security and Stability Audit
The codebase has been audited to address common low-level risks:
- **Resizing Resilience:** Implemented division-by-zero guards to prevent crashes during window minimization or extreme resizing.
- **Memory Hardening:** Clamped coloring indices to prevent out-of-bounds access and added overflow checks to pixel buffer allocations.
- **Error Handling:** Enhanced resource initialization paths to ensure clean-up on failure and improved diagnostic reporting.

Core logic is validated through a suite of automated unit tests.
- **Verification:** Run `cd tests && make` to verify core math and AVX2 consistency.
- **Continuous Integration:** Every modification is automatically built and tested via GitHub Actions on Ubuntu environments.

## Prerequisites

- C Compiler (GCC/Clang/MSVC with C11 support)
- CMake (3.10+, required for build system)
- Emscripten (emcc, required for Web target)
- SDL2 & SDL2_ttf development libraries (for CPU target)
- zlib & libpng (required for PNG screenshot export)

### Installation (Debian / Ubuntu)
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev zlib1g-dev libpng-dev
```

## Build & Run

You can compile the project using the provided `build.sh` wrapper script or standard CMake commands.

### Using Build Wrapper (Recommended)

```bash
chmod +x build.sh

./build.sh cpu    # Builds desktop version
./build.sh web    # Builds WebAssembly version
./build.sh gpu    # Builds Sokol GPU version
./build.sh clean  # Cleans build artifacts
```

### Using Standard CMake

```bash
# Build Native Desktop Engine (Default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build WebAssembly Target
emcmake cmake -S . -B build-web -DBUILD_WEB=ON
cmake --build build-web

# Build GPU Target
cmake -S . -B build-gpu -DBUILD_GPU=ON
cmake --build build-gpu
```

## Usage

| Action | Control |
|--------|---------|
| **Zoom In** | Left Mouse Drag (Selection box) |
| **Pan** | Right Mouse Drag |
| **Zoom at Cursor** | Mouse Wheel |
| **Undo Zoom** | `Ctrl` + `Z` |
| **Iterations** | `Up` / `Down` (Shift for x100, default x10) |
| **Palettes** | `P` |
| **Reset View** | `R` |
| **Toggle Julia Mode** | `J` |
| **Save Screenshot** | `S` |
| **Tour Mode** | `T` |
| **Quit** | `Esc` or `Q` |

## Project Structure

```text
.
├── core/                # Pure Mathematical Engine (Single Source of Truth)
├── cpu-engine/          # Desktop Renderer, SDL UI, and Thread Pools
├── gpu-engine/          # CUDA Kernel Implementations (.cu)
├── web-engine/          # Emscripten WebAssembly Runtime
├── include/             # Global configuration headers
├── tests/               # Automated regression frameworks
├── Makefile             # Multi-target Unified Build System
└── build.sh             # Cross-platform build abstraction
```

## Configuration

Rendering parameters can be tuned in `include/config.h` to balance performance and visual fidelity:

- `DEFAULT_ITERATIONS`: Controls the initial detail level.
- `MAX_ITERATIONS_LIMIT`: Upper bound for runtime adjustments.
- `THREAD_COUNT`: Number of parallel threads (set to match CPU cores).
- `ESCAPE_RADIUS`: Mathematical threshold for the set calculation.

## Roadmap

### Performance Optimization
- [x] Implement dynamic load balancing using atomic row-counters to maximize CPU utilization across all logical cores.
- [x] Integrate a pre-calculated Look-Up Table (LUT) for color mapping to bypass expensive real-time trigonometric calculations.
- [x] Implement smooth coloring algorithms using fractional iteration counts for high-fidelity gradients.
- [x] Deploy hardware-specific vectorization (AVX2 for Desktop, SIMD128 for WebAssembly) to process multiple pixels per cycle.
- [ ] Integrate `sokol_gfx` cross-platform GPU acceleration for extreme-scale rendering (Write-once, run-anywhere GPU logic).

### Features and Exploration
- [x] Add interactive runtime controls for iteration depth adjustment and dynamic color palette switching.
- [x] Implement automated "camera path" and "tour" modes for cinematic fractal exploration.
- [x] Connect HTML5 Frontend APIs strictly to the `web-engine` for a responsive, cross-platform user experience.
- [ ] Research and implement arbitrary-precision arithmetic to overcome the double-precision zoom limit (See [RESEARCH.md](RESEARCH.md)).

### Engineering and Quality
- [x] Establish a strict Engine-Centric Monorepo architecture, isolating platform rendering from core mathematical logic.
- [x] Implement a high-performance Multi-Target Master Makefile to streamline native and cross-compilation workflows.
- [x] Expand unit testing coverage to ensure mathematical consistency across all hardware backends.
- [x] Implement automatic CPU core detection to dynamically optimize thread pool allocation.

## Contributing

I am still learning, so if you spot any bugs or have suggestions for improvements (especially around memory safety or SIMD optimization!), I would really appreciate your help. Feel free to open an issue or pull request. Thank you!

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
