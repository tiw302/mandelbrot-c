# Mandelbrot-C

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/Language-C11-00599C.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
![GitHub repo size](https://img.shields.io/github/repo-size/tiw302/mandelbrot-c)
![GitHub last commit](https://img.shields.io/github/last-commit/tiw302/mandelbrot-c)

A simple, multi-threaded Mandelbrot and Julia set explorer written in C.
This is my attempt to learn low-level graphics and thread management. It is not perfect, but I tried to make it clean and fast.

---

## Table of Contents

- [Introduction](#mandelbrot-c)
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

Hi! I am currently diving into C programming and wanted to build something visual to understand pointers and memory better. This project is the result of my experiments with SDL2 and pthreads. I hope you find it interesting!

### Mandelbrot

![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot.png)
![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot2.png)
![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot3.png)

### julia

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
- **Hardware Acceleration:** AVX2 SIMD vectorization for processing 4 pixels in parallel per instruction.
- **Dynamic Multi-threading:** Intelligent row-based workload distribution using pthreads.
- **Julia Set Mode:** Press `J` to instantly switch to the Julia set defined by the point under your cursor.
- **Tour Mode:** Automated exploration paths for both Mandelbrot and Julia modes.
- **Screenshot Export:** PNG export functionality with no external library dependencies (uses integrated zlib).
- **Interactive Controls:** Advanced mouse-based panning and selection-based zooming.
- **Robustness:** Audited for memory safety and edge-case stability (stable during rapid window resizing).

## Technical Architecture

### Vectorized Optimization (SIMD)
The fractal engine utilizes 256-bit AVX2 registers. This allows the system to perform complex squaring, addition, and escape radius testing on 4 independent double-precision points simultaneously. On supported hardware, this represents a significant performance increase over scalar computation.

### Load Balancing
Parallelization is managed through a dynamic scheduling model. Unlike static partitioning, which can lead to inefficient CPU usage in regions of high iteration density, the engine uses atomic counters to allow threads to claim the next available row. This ensures uniform CPU utilization across all logical cores.

### Smooth Coloring and Performance
The implementation uses a fractional iteration counting algorithm to generate seamless color transitions. To ensure high frame rates, per-pixel trigonometric calls are replaced with a pre-calculated Look-Up Table (LUT) that is interpolated at runtime.

## Quality Assurance

### Security and Stability Audit
The codebase has been audited to address common low-level risks:
- **Resizing Resilience:** Implemented division-by-zero guards to prevent crashes during window minimization or extreme resizing.
- **Memory Hardening:** Clamped coloring indices to prevent out-of-bounds access and added overflow checks to pixel buffer allocations.
- **Error Handling:** Enhanced resource initialization paths to ensure clean-up on failure and improved diagnostic reporting.

### Automated Testing (CI/CD)
Core logic is validated through a suite of automated unit tests.
- **Verification:** Run `cd build && ctest` to verify core math and AVX2 consistency.
- **Continuous Integration:** Every modification is automatically built and tested via GitHub Actions on Ubuntu environments.

## Prerequisites

- C Compiler (GCC/Clang/MSVC with C11 support)
- CMake (version 3.10+)
- SDL2 & SDL2_ttf development libraries
- zlib (required for PNG screenshot export)

### Installation (Debian / Ubuntu)
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev zlib1g-dev
```

## Build & Run

A convenience build script is available:
```bash
chmod +x build.sh
./build.sh
```

Alternatively, use standard CMake commands:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mandelbrot
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
├── include/           # Header files and configuration
├── src/               # Core fractal engines and renderer
│   ├── mandelbrot.c   # AVX2 and Scalar Mandelbrot logic
│   ├── julia.c        # AVX2 and Scalar Julia logic
│   └── main.c         # Event loop and state management
├── tests/             # Automated verification (ctest)
└── docs/              # Technical deep-dives and GPU research
```

## Configuration

Rendering parameters can be tuned in `include/config.h` to balance performance and visual fidelity:

- `DEFAULT_ITERATIONS`: Controls the initial detail level.
- `MAX_ITERATIONS_LIMIT`: Upper bound for runtime adjustments.
- `THREAD_COUNT`: Number of parallel threads (set to match CPU cores).
- `ESCAPE_RADIUS`: Mathematical threshold for the set calculation.

## Roadmap

### Performance Optimization
- [x] Implement dynamic load balancing using a work queue or tiled rendering to improve CPU utilization across cores.
- [x] Replace real-time trigonometric color calculations with a pre-calculated Look-Up Table (LUT).
- [x] Implement smooth coloring algorithms using fractional iteration counts.
- [x] Explore SIMD (AVX/AVX2) vectorization to process multiple pixels per instruction.

### Features and Exploration
- [x] Add interactive controls to adjust maximum iterations and switch color palettes during runtime.
- [x] Implement an automated "camera path" or "tour" mode for smooth zooming animations.
- [ ] Research and implement high-precision arithmetic for deep zooms (See [RESEARCH.md](RESEARCH.md)).

### Advanced Backends (Experimental)
Future backend developments (GPU, WebAssembly) are currently being tracked and researched in separate branches. For more information on the architectural strategy, please refer to [RESEARCH.md](RESEARCH.md).

### Engineering Improvements
- [x] Migrate the build system from Makefile to CMake for better cross-platform support.
- [x] Add unit tests for core mathematical functions.
- [x] Implement automatic CPU core detection to dynamically set the thread count.

## Contributing

I am still learning, so if you spot any bugs or have suggestions for improvements (especially around memory safety!), I would really appreciate your help. Feel free to open an issue or pull request. Thank you!

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
