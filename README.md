# Mandelbrot-C

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/Language-C11-00599C.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
![GitHub repo size](https://img.shields.io/github/repo-size/tiw302/mandelbrot-c)
![GitHub last commit](https://img.shields.io/github/last-commit/tiw302/mandelbrot-c)

A high-performance, multi-threaded Mandelbrot and Julia set explorer written in C.
This project uses an Engine-Centric Architecture targeting Native Desktop (CPU/AVX2), Web (WebAssembly/SIMD128), and GPU (CUDA).

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
- **GPU Engine:** Houses CUDA kernels (`.cu`) structured for future extreme high-performance parallel GPU execution.

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

### Automated Testing (CI/CD)
Core logic is validated through a suite of automated unit tests.
- **Verification:** Run `cd tests && make` to verify core math and AVX2 consistency.
- **Continuous Integration:** Every modification is automatically built and tested via GitHub Actions on Ubuntu environments.

## Prerequisites

- C Compiler (GCC/Clang/MSVC with C11 support)
- Emscripten (emcc, required for Web target)
- CUDA Toolkit (nvcc, required for GPU target)
- SDL2 & SDL2_ttf development libraries (for CPU target)
- zlib & libpng (required for PNG screenshot export)

### Installation (Debian / Ubuntu)
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev zlib1g-dev libpng-dev
```

## Build & Run

A convenience build script wraps the Master Makefile logic:

```bash
chmod +x build.sh

# Build Native Desktop Engine (Default)
./build.sh cpu
./mandelbrot-desktop

# Build WebAssembly Target
./build.sh web
# Run an HTML server in your root directory to access mandelbrot.js

# Build GPU Target
./build.sh gpu
./mandelbrot-gpu
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
- [x] Implement dynamic load balancing using a work queue or tiled rendering.
- [x] Replace real-time trigonometric color calculations with a pre-calculated Look-Up Table (LUT).
- [x] Implement smooth coloring algorithms using fractional iteration counts.
- [x] Explore SIMD (AVX2/WASM SIMD128) vectorization to process multiple pixels per instruction.
- [ ] Activate CUDA `gpu-engine` parallel thread blocks logic.

### Features and Exploration
- [x] Add interactive controls to adjust maximum iterations and switch color palettes during runtime.
- [x] Implement an automated "camera path" or "tour" mode for smooth zooming animations.
- [ ] Connect HTML5 Frontend APIs strictly to the `web-engine`.

### Engineering
- [x] Establish a strict Engine-Centric Monorepo isolating platform rendering from core mathematics.
- [x] Deprecate CMake in favor of a Multi-Target Master Makefile.
- [x] Add unit testing coverage for mathematical engines.

## Contributing

I am still learning, so if you spot any bugs or have suggestions for improvements (especially around memory safety or SIMD optimization!), I would really appreciate your help. Feel free to open an issue or pull request. Thank you!

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
