# Mandelbrot-C

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/Language-C99-00599C.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Web-lightgrey.svg)](#platform-implementations)
[![SIMD](https://img.shields.io/badge/SIMD-AVX2%20%7C%20WASM--SIMD128-FF6B35.svg)](#platform-implementations)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-Supported-654FF0.svg)](https://tiw302.github.io/mandelbrot-c/)
[![Renderer](https://img.shields.io/badge/Renderer-CPU%20%7C%20OpenGL%20%7C%20WebGL2-00B4D8.svg)](#platform-implementations)
![GitHub repo size](https://img.shields.io/github/repo-size/tiw302/mandelbrot-c)
![GitHub last commit](https://img.shields.io/github/last-commit/tiw302/mandelbrot-c)

A high-performance, multi-threaded Mandelbrot and Julia set explorer written in C99. This project utilizes an Engine-Centric Architecture targeting Native Desktop (CPU/AVX2), Web (WebAssembly/SIMD128), and hardware-accelerated GPU rendering (WebGL/Sokol GFX).

**[Live Web Demo: tiw302.github.io/mandelbrot-c/](https://tiw302.github.io/mandelbrot-c/)**

---

## Table of Contents

| **Overview & UX** | **Engineering & Math** | **Dev & Ops** | **Project Lifecycle** |
| :--- | :--- | :--- | :--- |
| [Introduction](#introduction) | [The Mathematics](#the-mathematics) | [Prerequisites](#prerequisites) | [Roadmap](#roadmap) |
| [Technical Preview](#technical-preview) | [Technical Architecture](#technical-architecture) | [Build & Installation](#build-and-installation) | [Contributing](#contributing) |
| [Core Features](#core-features) | [Platform Implementations](#platform-implementations) | [Configuration](#configuration) | [License](#license) |
| [Interactive Controls](#interactive-controls) | | [Running Tests](#running-tests) | |
| | | [Project Structure](#project-structure) | |

---

## Introduction

Mandelbrot-C is an exploratory project focused on the intersection of low-level C programming and high-performance graphics. This journey began as a deep dive into C99 to understand pointers, memory management, and hardware acceleration. What started as a simple SDL2 experiment has evolved into a production-grade fractal engine.

Throughout the development process, I have explored advanced topics including SIMD intrinsics, multi-threaded load balancing, WebAssembly porting, and shader-based 64-bit precision emulation.

---

## Technical Preview

### Mandelbrot

![Mandelbrot Screenshot](assets/images/Mandelbrot-Screenshot.png)
![Mandelbrot Screenshot](assets/images/Mandelbrot-Screenshot2.png)
![Mandelbrot Screenshot](assets/images/Mandelbrot-Screenshot3.png)

### Julia Set Exploration

![Julia Screenshot](assets/images/julia-Screenshot.png)
![Julia Screenshot](assets/images/julia-Screenshot2.png)
![Julia Screenshot](assets/images/julia-Screenshot3.png)

---

## Core Features

- **Hybrid Rendering Pipeline:** Choice between optimized multi-threaded CPU rendering or hardware-accelerated GPU rendering.
- **WASM Performance:** Desktop-class performance in the browser via WebAssembly, SIMD128, and multi-threaded Web Workers.
- **Persistent State Sharing:** Share mathematical discoveries via URL parameters that encode the full view state. Clicking "Copy Link" generates and copies the URL on demand without constant address bar updates.

The URL format encodes the following parameters:

| Parameter | Format | Description |
| :--- | :--- | :--- |
| `re` / `im` | 14 decimal places | View center on the complex plane |
| `z` | Exponential (6 sig figs) | Zoom level |
| `it` | Integer | Iteration count |
| `p` | Integer | Palette index (0–5) |
| `j` | `1` if active | Julia mode flag |
| `jre` / `jim` | 14 decimal places | Julia set c-parameter (only present in Julia mode) |

Example: `?re=-0.74364388797764&im=0.13182590414575&z=1.234568e+4&it=500&p=0`
- **Hi-Lo Precision GPU Math:** 64-bit precision emulation in GLSL shaders for deep-zoom exploration without pixelation artifacts.
- **Interactive Tour Mode:** Automated exploration with two independent tour systems. The Mandelbrot tour cycles through 10 hand-picked deep-zoom coordinates using a three-phase sequence — Pan (1.8s), Zoom In (4.0s), Zoom Out (3.2s) — with smoothstep easing between phases and a zoom depth of 6000x. Both tours pick the next target randomly without repeating the previous one. On desktop, the Julia tour interpolates between 12 preset c-parameter keyframes (3.0s move, 1.2s dwell). On web, the Julia tour uses a continuous circular orbit (`c = 0.7885 × e^(it)`) for smooth real-time animation.
- **Professional Screenshot System:** Deferred capture logic that ensures high-fidelity PNG exports by synchronizing with the GPU rendering cycle. Both desktop and web save screenshots as `mandelbrot_YYYYMMDD_HHMMSS.png`. On desktop, stb_image_write handles PNG encoding with automatic ARGB-to-RGBA conversion. On web, the browser generates and downloads the file directly from the canvas.
- **Dynamic HUD:** A redesigned, responsive Heads-Up Display showing 14-decimal precision coordinates.

---

## Interactive Controls

| Action | Desktop Key | Web Key | Web UI / Touch |
| :--- | :--- | :--- | :--- |
| **Zoom In** | Left-Drag (Box) | Left-Drag (Box) / Scroll | Pinch-In |
| **Pan** | Right-Drag | Right-Drag | Two-Finger Drag |
| **Undo** | `Ctrl + Z` | `Ctrl + Z` | "Undo" Button |
| **Screenshot** | `S` | `S` | "Screenshot" Button |
| **Tour Mode** | `T` | `T` | "Tour" Button |
| **GPU/CPU Toggle** | `G` | `G` | "GPU" Button |
| **64-bit Precision** | - | `E` | "32-bit / 64-bit" Button (GPU only) |
| **Julia Toggle** | `J` | `J` | "Julia" Button |
| **Palette Cycle** | `P` | `P` | "Palette" Button |
| **Iterations** | `Up/Down` | `Up/Down` | `Iter+/Iter-` |
| **Reset View** | `R` | `R` | "Reset" Button |
| **Copy Link** | - | - | "Copy Link" Button |
| **Quit** | `Esc` / `Q` | - | - |

---

## The Mathematics

The Mandelbrot set is defined as the set of complex numbers $c$ for which the function $f_c(z) = z^2 + c$ remains bounded when iterated from $z = 0$.

### Optimization Strategies

To maintain high frame rates in dense regions, the engine implements several mathematical optimizations:

- **Main Cardioid Rejection:** Points inside the main cardioid are detected using a vectorized check to skip expensive iterations.
- **Period-2 Bulb Check:** Similar to the cardioid, points within the largest circular bulb are filtered out early.
- **Normalized Iteration Count:** Prevents color banding by using a fractional iteration formula, resulting in smooth gradients.

---

## Technical Architecture

### Engine-Centric Design

The codebase strictly adheres to a modular architecture to ensure Separation of Concerns (SoC):

- **Core [SSOT]:** Pure mathematical definitions (`mandelbrot.c`, `julia.c`) are the Single Source of Truth, agnostic to rendering APIs.
- **Engine Layer:** Manages high-level rendering logic, thread-pools, and platform-agnostic graphics abstractions (via Sokol GFX).
- **Application Layer:** Platform-specific entry points (SDL2 for Desktop, Emscripten for Web) handle input and OS-level interactions.

### WebAssembly Subsystem

The WASM implementation utilizes `SharedArrayBuffer` to enable real multi-threading in the browser. The built-in `server.py` is configured to handle the required COOP/COEP security headers for local development.

---

## Platform Implementations

### Platform Support

| Platform | Renderer | SIMD | Status |
| :--- | :--- | :--- | :--- |
| Linux | CPU / GPU (OpenGL) | AVX2 | Supported |
| macOS | CPU / GPU (OpenGL) | AVX2 | Supported |
| Windows | CPU / GPU (OpenGL) | AVX2 | Supported |
| Web (Browser) | CPU / GPU (WebGL 2.0) | SIMD128 | Supported |

### CPU Rendering (Native Desktop)

The native CPU engine is designed for maximum throughput on multi-core systems:

- **Dynamic Load Balancing:** Instead of static partitioning, the engine uses an **Atomic Row Counter**. Threads dynamically "claim" the next available row of pixels, ensuring that no CPU core sits idle while others are stuck rendering dense "black" regions of the fractal.
- **AVX2 Vectorization:** Utilizing 256-bit YMM registers, the engine processes **4 double-precision complex numbers** in a single instruction cycle (SIMD). This provides a theoretical 4x performance boost over scalar C code.
- **Persistent Thread Pool:** To avoid OS overhead, threads are spawned once at startup and managed via condition variables, ready to render new frames instantly as the user navigates. The thread count is capped at 64 regardless of core count. On WebAssembly, the engine always runs single-threaded due to platform constraints — multi-core Web Worker support is handled separately at the WASM subsystem level.

### Web Rendering (WebAssembly & WASM-SIMD)

Bringing desktop-class performance to the browser required solving several engineering challenges:

- **Multithreading via Web Workers:** By leveraging Emscripten's pthreads support, the C engine runs across multiple Web Workers. These workers communicate via a **SharedArrayBuffer**, allowing them to share the same pixel memory space as the main thread.
- **WASM-SIMD128:** We utilize the modern WebAssembly SIMD proposal (128-bit) to process **2 double-precision points** simultaneously, bridging the gap between browser and native performance.
- **Security & Headers:** To enable `SharedArrayBuffer`, the environment must be "Cross-Origin Isolated." We implemented a specialized **Service Worker** (`coi-serviceworker.js`) to automatically inject COOP and COEP headers, ensuring the engine runs on standard static hosting without server-side configuration.

### GPU Rendering (WebGL & Hi-Lo Precision)

The GPU path offloads all calculations to the graphics card for real-time smoothness. The shader is written in GLSL and compiled via Sokol's `sokol-shdc` annotation format (`@vs`, `@fs`, `@program`).

- **Hi-Lo Double Precision Emulation:** Each coordinate is passed to the shader as two `vec2` uniforms — `center_hi` and `center_lo`. The shader uses **Dekker double-single arithmetic** (`ds_add` + `ds_mul`) to perform full compensated addition and multiplication. This recovers ~48 mantissa bits from two 24-bit floats, achieving near-64-bit coordinate precision without hardware double support. Toggle between 32-bit and 64-bit mode at runtime with `E` on web.
- **Uniform Interface:** The fragment shader receives `center_hi`, `center_lo`, `zoom`, `iterations`, `aspect_ratio`, `palette_idx`, `julia_mode`, `julia_c_hi`, `julia_c_lo`, and `high_precision` — giving the CPU full control over every rendering parameter per frame.
- **All 6 Palettes in Shader:** The GLSL palette function exactly replicates the fractional iteration interpolation from `color.c`, ensuring GPU and CPU renders are visually identical when switching modes.
- **Cardioid and Period-2 Bulb Rejection:** The shader performs the same early-exit checks as the CPU scalar path, skipping the iteration loop entirely for points confirmed inside the main set.
- **Julia Set Support:** A `julia_mode` uniform switches the shader between Mandelbrot (z₀ = 0, c = pixel) and Julia (z₀ = pixel, c = fixed parameter passed as `julia_c_hi + julia_c_lo`).
- **Correct Escape Radius:** The shader uses `ESCAPE_RADIUS = 10.0` matching `config.h`, consistent with the CPU path.
- **Sokol GFX Integration:** The same shader and pipeline logic runs on Native OpenGL (Desktop) and WebGL 2.0 (Browser) via Sokol GFX.
- **Deferred Readback:** Screenshots in GPU mode utilize a "Deferred Capture" system, ensuring the pixel data is read back from the GPU memory only after the frame is fully validated.

---

## Prerequisites

Before building, ensure the following tools and libraries are installed on your system.

### Desktop — CPU Engine

| Dependency | Version | Notes |
| :--- | :--- | :--- |
| GCC / Clang | GCC 9+ / Clang 10+ | C99 support required |
| CMake | 3.10+ | Build system |
| SDL2 | 2.0.14+ | Windowing and input |
| SDL2_ttf | 2.0+ | Font rendering for HUD |
| libGL / OpenGL | 3.3+ | Required for Sokol GFX |

**Linux (Debian/Ubuntu):**
```bash
sudo apt install cmake libsdl2-dev libsdl2-ttf-dev libgl1-mesa-dev
```

**macOS (Homebrew):**
```bash
brew install cmake sdl2 sdl2_ttf
```

### Desktop — GPU Engine

| Dependency | Version | Notes |
| :--- | :--- | :--- |
| GCC / Clang | GCC 9+ / Clang 10+ | C99 support required |
| CMake | 3.10+ | Build system |
| libGL / OpenGL | 3.3+ | Required for Sokol GFX |

> The GPU engine does not depend on SDL2 or SDL2_ttf.

**Linux (Debian/Ubuntu):**
```bash
sudo apt install cmake libgl1-mesa-dev
```

**macOS (Homebrew):**
```bash
brew install cmake
```

### Web (WebAssembly)

| Dependency | Version | Notes |
| :--- | :--- | :--- |
| Emscripten | 3.1.0+ | WASM compiler toolchain |
| Python | 3.x | Required for `server.py` |

Follow the [Emscripten installation guide](https://emscripten.org/docs/getting_started/downloads.html) and ensure `emcmake` is available in your PATH.

---

## Build and Installation

### Interactive TUI Build (Recommended)

Run `./build.sh` without arguments for a numbered menu:
```bash
./build.sh
```

### CLI Build

Pass a target directly to skip the menu:

| Command | Action |
| :--- | :--- |
| `./build.sh cpu` | Build CPU engine only |
| `./build.sh gpu` | Build GPU engine only |
| `./build.sh web` | Build web (WASM) engine only |
| `./build.sh all` | Build all three targets |
| `./build.sh clean` | Remove all build directories |

### Manual Build

```bash
# Desktop — CPU engine
cmake -S . -B build-cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-cpu
./build-cpu/mandelbrot-cpu

# Desktop — GPU engine
cmake -S . -B build-gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gpu
./build-gpu/mandelbrot-gpu

# Web (WASM)
emcmake cmake -S . -B build-web -DBUILD_WEB=ON
cmake --build build-web
# Output is automatically copied to the deploy/ folder
```

### Running the Web Build Locally

The web build requires specific HTTP security headers (`COOP`/`COEP`) to enable `SharedArrayBuffer`. Use the included server script:
```bash
python3 server.py
```
Then open `http://localhost:8081` in your browser.

Optional arguments:

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--port` | `8081` | Port to listen on |
| `--dir` | `web` | Directory to serve |

```bash
# Example: serve the deploy/ folder on port 9000
python3 server.py --dir deploy --port 9000
```

---

## Configuration

Rendering parameters can be tuned in `include/config.h`:

| Parameter | Default | Description |
| :--- | :--- | :--- |
| `WINDOW_WIDTH` / `WINDOW_HEIGHT` | `1024` / `768` | Initial window resolution |
| `DEFAULT_ITERATIONS` | `500` | Initial iteration depth |
| `MAX_ITERATIONS_LIMIT` | `10000` | Upper bound for runtime adjustments |
| `DEFAULT_THREAD_COUNT` | `4` | Number of parallel threads (`0` = auto-detect from CPU cores, max 64) |
| `ESCAPE_RADIUS` | `10` | Mathematical threshold for divergence |
| `DEFAULT_PALETTE` | `0` | Starting color palette index (see table below) |
| `INITIAL_CENTER_RE` / `INITIAL_CENTER_IM` | `-0.5` / `0.0` | Initial view center (complex plane) |
| `INITIAL_ZOOM` | `3.0` | Initial zoom level |
| `MAX_HISTORY_SIZE` | `100` | Maximum undo history depth |

### Color Palettes

The engine ships with 6 built-in palettes, selectable at runtime with `P` or `Iter+/Iter-`, or set as default via `DEFAULT_PALETTE` in `config.h`:

| Index | Name | Character |
| :--- | :--- | :--- |
| `0` | Sine Wave | Smooth cycling colors using sine-wave interpolation |
| `1` | Grayscale | Pure luminance, iteration count mapped to brightness |
| `2` | Fire | Blue-to-white ramp, cool-to-hot gradient |
| `3` | Electric | Red-dominant, high-contrast neon feel |
| `4` | Ocean | Warm amber tones with subtle blue undertones |
| `5` | Inferno | Deep blue-to-white, high-zoom detail emphasis |

All palettes use fractional iteration interpolation to eliminate color banding at region boundaries.

---

## Running Tests

The test suite covers core mathematical correctness and AVX2 vs scalar consistency. Tests are located in the `tests/` directory and use a standalone Makefile.

```bash
cd tests
make
```

Running `make` will compile and execute the tests automatically. On machines with AVX2 support, the compiler detects it at build time and includes additional vectorization consistency tests.

| Test | Description |
| :--- | :--- |
| `mandelbrot_check basics` | Verifies cardioid, period-2 bulb, and escape behavior |
| `julia_check basics` | Verifies bounded and divergent Julia set points |
| `avx2 vs scalar consistency` | Confirms AVX2 results match scalar output within `1e-7` |

To clean up the test binary:
```bash
make clean
```

> AVX2 tests are compiled and run automatically if the host CPU supports it. On machines without AVX2, those tests are skipped and reported as such.

---

## Project Structure

```text
.
├── include/             # Global configuration and platform headers
├── src/
│   ├── core/           # Pure Mathematical Engine (Single Source of Truth)
│   ├── engine/         # Platform-Agnostic Renderers, Tours, and Logic
│   └── app/            # Platform-Specific Entries (Desktop, Web)
├── shaders/             # GLSL shader source files
├── web/                 # Web Frontend (HTML, CSS, JS)
├── assets/              # Shared Typography and Media
├── tests/               # Automated Unit Testing Suite
├── third_party/         # External Abstractions (Sokol, stb, etc.)
├── deploy/              # Generated by web build — ready-to-serve package
├── CMakeLists.txt       # Unified Cross-platform Build System
└── build.sh             # Interactive TUI Build Wrapper
```

---

## Roadmap

### Performance Optimization
- [x] Implement dynamic load balancing using atomic row-counters to maximize CPU utilization.
- [x] Integrate a pre-calculated Look-Up Table (LUT) for color mapping.
- [x] Implement smooth coloring algorithms using fractional iteration counts.
- [x] Deploy hardware-specific vectorization (AVX2 for Desktop, SIMD128 for WebAssembly).
- [x] Research and implement pure-shader fractal calculation for GPU rendering.
- [x] Optimize Julia set calculation using hardware-specific vectorization.

### Features and Exploration
- [x] Add interactive runtime controls for iteration depth and palette switching.
- [x] Implement automated "camera path" and "tour" modes.
- [x] Connect HTML5 Frontend APIs to the web-engine for a responsive experience.
- [x] Implement URL-based state recovery and deep-linking for sharing discoveries.
- [x] Add mobile touch support (pinch-to-zoom and gesture-based panning).

### Engineering and Quality
- [x] Establish a strict Engine-Centric Monorepo architecture.
- [x] Implement a high-performance CMake build system.
- [x] Expand unit testing coverage to ensure mathematical consistency.
- [x] Implement automatic CPU core detection for dynamic thread pool allocation.
- [x] Implement Hi-Lo 64-bit precision emulation for GPU shaders.
- [ ] Research and implement arbitrary-precision arithmetic for infinite zoom.

---

## Contributing

Contributions, bug reports, and suggestions are welcome. Areas of particular interest include memory safety, SIMD optimization, and GPGPU improvements.

To contribute:
1. Open an **issue** to discuss bugs or proposed changes.
2. **Fork** the repository and open a **pull request** with your changes.
3. Descriptive commit messages and clear explanations are appreciated.

---

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
