# Mandelbrot-C

[![Linux](https://github.com/tiw302/mandelbrot-c/actions/workflows/linux.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/linux.yml) [![macOS](https://github.com/tiw302/mandelbrot-c/actions/workflows/macos.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/macos.yml) [![Windows](https://github.com/tiw302/mandelbrot-c/actions/workflows/windows.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/windows.yml) [![WebAssembly](https://github.com/tiw302/mandelbrot-c/actions/workflows/wasm.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/wasm.yml) [![CodeQL](https://github.com/tiw302/mandelbrot-c/actions/workflows/codeql.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/codeql.yml) [![Memory Check](https://github.com/tiw302/mandelbrot-c/actions/workflows/memcheck.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/memcheck.yml) [![Formatting](https://github.com/tiw302/mandelbrot-c/actions/workflows/format.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions/workflows/format.yml)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg?logo=opensourceinitiative&logoColor=white)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/Language-C99-00599C.svg?logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Web-lightgrey.svg?logo=linux&logoColor=white)](#platform-implementations)
[![SIMD](https://img.shields.io/badge/SIMD-AVX2%20%7C%20WASM--SIMD128-FF6B35.svg?logo=intel&logoColor=white)](#platform-implementations)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-Supported-654FF0.svg?logo=webassembly&logoColor=white)](https://tiw302.github.io/mandelbrot-c/)
[![Renderer](https://img.shields.io/badge/Renderer-CPU%20%7C%20OpenGL%20%7C%20WebGL2-00B4D8.svg?logo=opengl&logoColor=white)](#platform-implementations)
![GitHub repo size](https://img.shields.io/github/repo-size/tiw302/mandelbrot-c?logo=github&logoColor=white)
![GitHub last commit](https://img.shields.io/github/last-commit/tiw302/mandelbrot-c?logo=github&logoColor=white)

A high-performance, multi-threaded fractal explorer written in C99. Supports **6 fractal types** (Mandelbrot, Julia, Burning Ship, Tricorn, Celtic, Buffalo), **22 color palettes**, and an Engine-Centric Architecture targeting Native Desktop (CPU/AVX2/AVX-512/NEON), Web (WebAssembly/SIMD128), and hardware-accelerated GPU rendering (OpenGL/WebGL via Sokol GFX).

**[Live Web Demo: tiw302.github.io/mandelbrot-c/](https://tiw302.github.io/mandelbrot-c/)**

---

## Table of Contents

- **Getting Started**
  - [Quick Start](#quick-start)
  - [Introduction](#introduction)
  - [Technical Preview](#technical-preview)
- **Features**
  - [Core Features](#core-features)
  - [Interactive Controls](#interactive-controls)
- **Engineering**
  - [The Mathematics](#the-mathematics)
  - [Technical Architecture](#technical-architecture)
  - [How a Frame is Rendered](#how-a-frame-is-rendered)
  - [Platform Implementations](#platform-implementations)
  - [Precision Ladder](#precision-ladder)
  - [Performance Deep-Dive](#performance-deep-dive)
  - [Performance Benchmarks](#performance-benchmarks)
  - [Benchmark Methodology](#benchmark-methodology)
- **Exploration**
  - [Notable Zoom Coordinates](#notable-zoom-coordinates)
  - [Web vs Desktop Feature Matrix](#web-vs-desktop-feature-matrix)
- **Development**
  - [Prerequisites](#prerequisites)
  - [Build & Installation](#build-and-installation)
  - [Configuration](#configuration)
  - [Running Tests](#running-tests)
  - [Project Structure](#project-structure)
- **Project**
  - [Roadmap](#roadmap)
  - [Contributing](#contributing)
  - [Development Methodology & AI Assistance](#development-methodology--ai-assistance)
  - [Author's Note](#authors-note)
  - [License](#license)

---

## Quick Start

```bash
# 1. Clone the repository
git clone https://github.com/tiw302/mandelbrot-c.git && cd mandelbrot-c

# 2. Build (interactive menu — pick CPU, GPU, or Web)
./build.sh

# 3. Run
./build_cpu/mandelbrot_cpu   # CPU engine
./build_gpu/mandelbrot_gpu   # GPU engine (requires OpenGL 3.3+)
```

For the Web build, see [Build & Installation](#build-and-installation).

---

## Introduction

Mandelbrot-C is an exploratory project focused on the intersection of low-level C programming and high-performance graphics. This journey began as a deep dive into C99 to understand pointers, memory management, and hardware acceleration. What started as a simple SDL2 experiment has evolved into a production-grade fractal engine with 6 fractal types, a pluggable fractal registry, 22 color palettes, and a Mandelbrot Video Studio.

Throughout the development process, I have explored advanced topics including SIMD intrinsics (AVX2, AVX-512, WASM-SIMD128, ARM NEON), multi-threaded load balancing, perturbation theory for deep zoom, WebAssembly porting, and shader-based 64-bit precision emulation.

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

- **6 Fractal Types via Pluggable Registry:** The fractal engine uses a central registry (`fractal.c`) to dispatch math kernels at runtime. All 6 fractal types share the same SIMD-accelerated and 128-bit precision infrastructure:

| Fractal | Description | Default Center |
| :--- | :--- | :--- |
| Mandelbrot | Classic $z^2 + c$, the flagship fractal | $(-0.5, 0)$ |
| Julia | Fixed $c$ parameter, pixel as $z_0$ | $(0, 0)$ |
| Burning Ship | $z \leftarrow (|\text{Re}(z)| + i|\text{Im}(z)|)^2 + c$ | $(-0.4, -0.6)$ |
| Tricorn | Conjugate Mandelbrot: $\bar{z}^2 + c$ | $(-0.1, 0)$ |
| Celtic | Celtic variant: $|\text{Re}(z^2)| + \text{Im}(z^2) + c$ | $(-0.5, 0)$ |
| Buffalo | Buffalo variant: $|z|^2 + c$ with absolute value | $(-0.4, 0)$ |

- **Hybrid Rendering Pipeline:** Choice between optimized multi-threaded CPU rendering or hardware-accelerated GPU rendering, switchable at runtime with `G`.
- **WASM Performance:** Desktop-class performance in the browser via WebAssembly, SIMD128, and multi-threaded Web Workers.
- **Persistent State Sharing:** Share mathematical discoveries via URL parameters. Clicking "Copy Link" generates and copies the URL on demand.

The URL format encodes the following parameters:

| Parameter | Format | Description |
| :--- | :--- | :--- |
| `re` / `im` | 14 decimal places | View center on the complex plane |
| `z` | Exponential (6 sig figs) | Zoom level |
| `it` | Integer | Iteration count |
| `p` | Integer | Palette index (0–21) |
| `f` | Integer | Base fractal mode index |
| `j` | `1` if active | Julia mode flag |
| `jre` / `jim` | 14 decimal places | Julia set c-parameter (only present in Julia mode) |

Example: `?re=-0.74364388797764&im=0.13182590414575&z=1.234568e+4&it=500&p=0&f=0`

- **Hi-Lo Precision GPU Math:** 64-bit precision emulation in GLSL shaders (Dekker double-single arithmetic) for deep-zoom without pixelation artifacts.
- **Perturbation Theory Engine:** A reference-orbit system (`perturbation.c`) enables accurate rendering at extreme zoom depths ($>10^{14}$) by computing a single high-precision center orbit on the CPU, then calculating all other pixels as low-precision deltas. Includes Series Approximation (SA) coefficients (A, B, C) for skipping early iterations.
- **Interactive Tour Mode:** Automated exploration with two independent tour systems. The Mandelbrot tour cycles through 10 hand-picked deep-zoom coordinates using a three-phase sequence — Pan (1.8s), Zoom In (4.0s), Zoom Out (3.2s) — with smoothstep easing and a zoom depth of 6000×. On desktop, the Julia tour interpolates between 12 preset c-parameter keyframes. On web, the Julia tour uses a continuous circular orbit (`c = 0.7885 × e^(it)`).
- **Mandelbrot Video Studio:** A dedicated GUI application (`mandelbrot_video`) for rendering fractal zoom animations to `.mp4` via FFmpeg. Supports GUI and headless CLI modes, scenic/bookmark/custom paths, supersampling AA, and telemetry overlay. See [README_VIDEO_STUDIO.md](README_VIDEO_STUDIO.md) for full documentation.
- **Professional Screenshot System:** Deferred GPU readback ensures correct frame capture. Desktop saves via `stb_image_write` with ARGB→RGBA conversion. Web downloads directly from the canvas. Filename format: `mandelbrot_YYYYMMDD_HHMMSS.png`.
- **Mega Screenshot (8K):** Desktop-only `X` key triggers a tiled 8K render using the CPU engine, exported as a `.tga` file.
- **Dynamic HUD:** Responsive Heads-Up Display showing 14-decimal precision coordinates, palette name, engine mode, and iteration count.

---

## Interactive Controls

| Action | Desktop Key | Web Key | Web UI / Touch |
| :--- | :--- | :--- | :--- |
| **Zoom In** | Left-Drag (Box) | Left-Drag (Box) / Scroll | Pinch-In |
| **Pan** | Right-Drag | Right-Drag | Two-Finger Drag |
| **Undo** | `Ctrl + Z` | `Ctrl + Z` | "Undo" Button |
| **Screenshot** | `S` | `S` | "Screenshot" Button |
| **Mega Screenshot (8K)** | `X` | - | - |
| **Record Video** | `V` | - | - |
| **Tour Mode** | `T` | `T` | "Tour" Button |
| **GPU/CPU Toggle** | `G` | `G` | "GPU" Button |
| **Precision Toggle** | `E` (CPU: 64/128-bit, GPU: 32/64-bit) | `E` | "32-bit / 64-bit" Button |
| **Julia Toggle** | `J` | `J` | "Julia" Button |
| **Cycle Fractal Type** | `F` | `F` / `B` | "Fractal" Button / Mode Select |
| **Palette Cycle** | `P` | `P` | "Palette" Button / Palette Select |
| **Iterations** | `Up/Down` (`Shift` ×100) | `Up/Down` | `Iter+/Iter-` |
| **Save Bookmark** | `M` | - | - |
| **Load Bookmark** | `L` | - | - |
| **Reset View** | `R` | `R` | "Reset" Button |
| **Copy Link** | - | `Ctrl+C` / Button | "Copy Link" Button |
| **About / Info** | - | - | "About ℹ" Button |
| **Settings** | - | - | "Settings ⚙" Button |
| **Quit** | `Esc` / `Q` | - | - |

> [!NOTE]
> The **About ℹ** panel explains the project overview and highlights differences between the Web and Desktop builds (e.g., features unavailable in the browser such as Mega Screenshot, Bookmark, and Video Studio).

---

## The Mathematics

The Mandelbrot set is defined as the set of complex numbers $c$ for which the function $f_c(z) = z^2 + c$ remains bounded when iterated from $z = 0$.

### Supported Fractal Formulas

| Fractal | Iteration Formula |
| :--- | :--- |
| Mandelbrot | $z_{n+1} = z_n^2 + c$ |
| Julia | $z_{n+1} = z_n^2 + c_{\text{fixed}}$, $z_0 = $ pixel |
| Burning Ship | $z_{n+1} = (|\text{Re}(z_n)| + i|\text{Im}(z_n)|)^2 + c$ |
| Tricorn | $z_{n+1} = \bar{z}_n^2 + c$ (conjugate) |
| Celtic | $z_{n+1} = |\text{Re}(z_n^2)| + i\,\text{Im}(z_n^2) + c$ |
| Buffalo | $z_{n+1} = |z_n|^2 + c$ |

### Optimization Strategies

To maintain high frame rates in dense regions, the engine implements several mathematical optimizations:

- **Main Cardioid Rejection:** Points inside the main cardioid are detected using a vectorized check to skip expensive iterations.
- **Period-2 Bulb Check:** Similar to the cardioid, points within the largest circular bulb are filtered out early.
- **Normalized Iteration Count:** Prevents color banding by using a fractional iteration formula, resulting in smooth gradients.
- **Perturbation Theory:** For extreme zoom depths ($>10^{14}$) where 64-bit doubles lose precision, the engine computes a single high-precision reference orbit at the screen center. All other pixels are rendered as low-precision complex deltas relative to that orbit — enabling infinite-precision-like rendering using standard GPU floats.
  - *Note on Deep Zoom Limits:* Despite perturbation theory, this engine employs a **hard zoom limit at $1e^{-14}$**. Pushing the GPU beyond this threshold without advanced error-correction leads to catastrophic precision loss (glitches and artifacts) because the GPU's `double-single` math emulation cannot track deltas smaller than $1e^{-14}$.
- **Series Approximation (SA) [Not Implemented]:** To truly bypass the $1e^{-14}$ barrier and achieve infinite zoom (e.g., $1e^{-100}$), an architecture known as Series Approximation (Taylor Series / Bilinear Approximation) is required.
  - *Why we stopped here:* SA requires a massive architectural shift. The CPU must compute complex polynomial derivatives in `bignum` to extract Taylor coefficients (A, B, C), and transfer them via SSBOs to the GPU, allowing the shader to skip thousands of iterations at once. While this grants incredible speed and infinite zoom, it introduces immense complexity, heavy CPU bottlenecks in distorted regions, and bandwidth issues on WebGL platforms. Since this project focuses on a clean, simple, and educational C99 codebase rather than competing with extreme deep-zoomers like *Kalles Fraktaler*, we opted to hard-limit the camera at $1e^{-14}$ (which is still a 100-trillion-times zoom) to preserve code simplicity.

---

## Technical Architecture

### Engine-Centric Design

The codebase strictly adheres to a modular architecture to ensure Separation of Concerns (SoC):

- **Core [SSOT]:** Pure mathematical definitions (`mandelbrot.c`, `julia.c`, `burning_ship.c`, `tricorn.c`, `celtic.c`, `buffalo.c`, `color.c`) are the Single Source of Truth, agnostic to rendering APIs. A central **Fractal Registry** (`fractal.c`) connects all fractal math kernels to the SIMD dispatch layer. Also includes `bignum.c` and `mandelbrot_bignum.c` — a software arbitrary-precision integer engine for research into infinite zoom.
- **Engine Layer:** Manages high-level rendering logic, thread-pools, perturbation theory (`perturbation.c`), tour system, bookmark I/O, screenshot pipeline, app state machine, `config_loader.c` for persistent settings, and platform-agnostic graphics abstractions (via Sokol GFX).
- **UI Layer:** HUD rendering split by backend — `hud_sdl.c` (CPU desktop), `hud_sokol.c` (GPU/Web), `hud_settings.c` (settings panel), and `hud_video_studio.c` (Video Studio GUI). Dear ImGui integration via `cimgui` is available for the Video Studio.
- **Application Layer:** Platform-specific thin entry points (Sokol App for Desktop, Emscripten for Web) handle input and OS-level interactions. Each target compiles a separate `_main.c` file to avoid `#ifdef` proliferation.

### WebAssembly Subsystem

The WASM implementation utilizes `SharedArrayBuffer` to enable real multi-threading in the browser. The built-in `scripts/server.py` is configured to handle the required COOP/COEP security headers for local development.

---

## How a Frame is Rendered

Each frame follows a different path depending on the active engine. Here is what happens from input to pixel:

### CPU Path

```
User Input
    └─> app_runner.c (Sokol event)
            └─> input_handler.c (key/mouse dispatch)
                    └─> app_state.c (update center, zoom, palette, fractal type)
                            └─> renderer.c (dispatch to thread pool)
                                    ├─> Thread 0..N: claim next row via atomic counter
                                    │       └─> fractal.c registry → math kernel
                                    │               ├─> mandelbrot_check_avx2()   (AVX2)
                                    │               ├─> mandelbrot_check_avx512() (AVX-512)
                                    │               ├─> mandelbrot_check_neon()   (ARM NEON)
                                    │               └─> mandelbrot_check()        (scalar fallback)
                                    └─> pixel buffer (ARGB) → color.c LUT → screen
```

### GPU Path

```
User Input
    └─> app_runner.c (Sokol event)
            └─> input_handler.c
                    └─> app_state.c (update uniforms)
                            ├─> [if zoom < 1e-6] perturbation.c
                            │       ├─> compute reference orbit (CPU, double precision)
                            │       ├─> compute SA coefficients A, B, C
                            │       └─> upload orbit as GPU texture
                            └─> Sokol GFX draw call
                                    └─> mandelbrot.glsl fragment shader
                                            ├─> receive uniforms: center_hi/lo, zoom, iterations,
                                            │   palette_idx, julia_mode, base_fractal_mode, high_precision
                                            ├─> [hi_precision] Dekker ds_add + ds_mul → ~48-bit coords
                                            ├─> [perturbation] delta arithmetic d_{n+1} = 2w_n*d_n + d_n^2 + Δc
                                            └─> fractal iteration → smooth color → gl_FragColor
```

> [!NOTE]
> The thread pool is **persistent** — threads are spawned once at startup and wait on condition variables between frames. This eliminates thread creation overhead entirely and keeps frame latency predictable.

---

## Platform Implementations

### Platform Support

| Platform | Renderer | SIMD | Status |
| :--- | :--- | :--- | :--- |
| Linux | CPU / GPU (OpenGL) | AVX-512 / AVX2 | Supported |
| macOS | CPU / GPU (OpenGL) | AVX-512 / AVX2 | Supported |
| Windows | CPU / GPU (OpenGL) | AVX-512 / AVX2 | Supported |
| Web (Browser) | CPU / GPU (WebGL 2.0) | SIMD128 | Supported |

### CPU Rendering (Native Desktop)

The native CPU engine is designed for maximum throughput on multi-core systems:

- **Dynamic Load Balancing:** Instead of static partitioning, the engine uses an **Atomic Row Counter**. Threads dynamically "claim" the next available row of pixels, ensuring that no CPU core sits idle while others are stuck rendering dense "black" regions of the fractal.
- **SIMD Vectorization:** The engine auto-selects the best available SIMD path at compile time:
  - **AVX-512:** Processes **8 double-precision numbers** per cycle on capable CPUs
  - **AVX2:** Processes **4 double-precision numbers** per cycle via 256-bit YMM registers (4× scalar throughput)
  - **ARM NEON:** Processes **2 double-precision numbers** per cycle on Apple Silicon and other ARM64 systems
  - **WASM-SIMD128:** Processes **2 double-precision numbers** per cycle in the browser
- **Persistent Thread Pool:** Threads are spawned once at startup and managed via condition variables, ready to render new frames instantly. The thread count is capped at 64 regardless of core count. On WebAssembly, single-threaded mode is used — multi-core Web Worker support is handled at the WASM subsystem level.
- **All 6 fractal types** share the same SIMD dispatch infrastructure via the fractal registry — there is no separate SIMD implementation per fractal.

### Web Rendering (WebAssembly & WASM-SIMD)

Bringing desktop-class performance to the browser required solving several engineering challenges:

- **Multithreading via Web Workers:** By leveraging Emscripten's pthreads support, the C engine runs across multiple Web Workers. These workers communicate via a **SharedArrayBuffer**, allowing them to share the same pixel memory space as the main thread.
- **WASM-SIMD128:** We utilize the modern WebAssembly SIMD proposal (128-bit) to process **2 double-precision points** simultaneously, bridging the gap between browser and native performance.
- **Security & Headers:** To enable `SharedArrayBuffer`, the environment must be "Cross-Origin Isolated." We implemented a specialized **Service Worker** (`coi-serviceworker.js`) to automatically inject COOP and COEP headers, ensuring the engine runs on standard static hosting without server-side configuration.

### GPU Rendering (WebGL & Hi-Lo Precision)

The GPU path offloads all calculations to the graphics card for real-time smoothness. The shader is written in GLSL and compiled via Sokol's `sokol-shdc` annotation format (`@vs`, `@fs`, `@program`).

- **Hi-Lo Double Precision Emulation:** Each coordinate is passed to the shader as two `vec2` uniforms — `center_hi` and `center_lo`. The shader uses **Dekker double-single arithmetic** (`ds_add` + `ds_mul`) to perform full compensated addition and multiplication. This recovers ~48 mantissa bits from two 24-bit floats, achieving near-64-bit coordinate precision without hardware double support. Toggle between 32-bit and 64-bit mode at runtime with `E`.
- **Full Uniform Interface:** The fragment shader receives `center_hi`, `center_lo`, `zoom`, `iterations`, `aspect_ratio`, `palette_idx`, `julia_mode`, `julia_c_hi`, `julia_c_lo`, `base_fractal_mode`, and `high_precision` — giving the CPU full control over every rendering parameter per frame, including fractal type selection.
- **All 22 Palettes in Shader:** The GLSL palette function replicates all 22 palettes from `color.c`. GPU-exclusive palettes use advanced shader techniques (orbit traps, conformal mappings, domain coloring) unavailable on the CPU path.
- **All 6 Fractal Types in Shader:** The fragment shader implements all 6 fractal formulas, switchable via the `base_fractal_mode` uniform with no pipeline rebuild required.
- **Cardioid and Period-2 Bulb Rejection:** The shader performs the same early-exit checks as the CPU scalar path, skipping the iteration loop entirely for points confirmed inside the main set.
- **Sokol GFX Integration:** The same shader and pipeline logic runs on Native OpenGL 3.3 (Desktop) and WebGL 2.0 (Browser) via Sokol GFX. Shaders are compiled via `sokol-shdc` from annotated GLSL source (`@vs`, `@fs`, `@program`).
- **Deferred Readback:** Screenshots in GPU mode use a "Deferred Capture" system, ensuring pixel data is read from GPU memory only after the frame is fully validated and rendered.

---

## Precision Ladder

One of the core engineering challenges in a fractal renderer is floating-point precision. As you zoom deeper, standard 64-bit doubles run out of significant digits and the image pixelates. This engine solves this through a layered precision stack, where each level trades speed for accuracy.

| Level | Mode | Mantissa Bits | Max Useful Zoom | Speed (1080p, 8 cores) |
| :--- | :--- | :--- | :--- | :--- |
| 32-bit float | GPU default (`E` off) | ~24 bits | ~$10^4$ | ~79 fps |
| 64-bit double | CPU/GPU Hi-Lo Dekker | ~48–53 bits | ~$10^{12}$ | ~115 fps (AVX2) |
| 128-bit double-double | CPU `simd-f128` (`E` on desktop) | ~106 bits | ~$10^{24}$ | ~16 fps |
| BigNum (arbitrary) | CPU single ref point only | Configurable | Theoretical $\infty$ | Research only |

> [!IMPORTANT]
> BigNum is used exclusively for computing the single **reference orbit** in Perturbation Theory — not per-pixel. All other pixels are rendered as fast 32-bit delta offsets relative to that reference point, which is why extreme zoom remains interactive.

The engine auto-selects the right level: Perturbation Theory activates automatically when `zoom < PERTURBATION_ZOOM_THRESHOLD` (`1e-6`) on Mandelbrot GPU mode. The precision mode can be toggled at runtime with `E`.

---

## Performance Deep-Dive

Every optimization in the engine targets a specific bottleneck. Here is what each technique actually contributes:

| Optimization | Technique | Speedup vs Baseline |
| :--- | :--- | :--- |
| Cardioid + period-2 bulb rejection | Skip iteration loop for guaranteed-interior points | ~2–4× on default views |
| Fractional iteration (smooth coloring) | `log2(log(mag_sq))` normalized count | No speed cost; eliminates banding |
| Color LUT | Pre-computed ARGB table per palette | Removes palette math from hot loop |
| AVX2 SIMD | Process 4 doubles per cycle via YMM registers | ~3.7× over scalar |
| AVX-512 SIMD | Process 8 doubles per cycle via ZMM registers | ~7× over scalar |
| ARM NEON | Process 2 doubles per cycle on Apple Silicon / ARM64 | ~1.9× over scalar |
| Persistent thread pool (8 cores) | Atomic row counter, no static partitioning | ~7.5× over single-threaded |
| Atomic row counter load balancing | Threads claim next free row dynamically | Eliminates idle cores near black regions |
| simd-f128 128-bit double-double | Software compensated arithmetic via AVX2 | Enables $10^{24}$ zoom at ~16 fps |
| Perturbation Theory + SA | Single ref orbit + fast delta math per pixel | Enables $10^{14}$+ zoom at interactive fps |
| GPU shader (32-bit) | Full render on graphics card | ~79 fps (no CPU iteration cost) |
| GPU Hi-Lo Dekker | 2×`vec2` uniforms emulate 48-bit mantissa | Near-64-bit GPU zoom at ~60 fps |

> [!TIP]
> The biggest single win for typical use is the **combination of AVX2 + persistent thread pool**: together they deliver roughly **28× the throughput** of a naive single-threaded scalar loop on an 8-core machine.

---

## Performance Benchmarks

The following numbers were measured on a Linux system with an Intel CPU (AVX2-capable) and an integrated GPU. Results will vary by hardware.

### CPU Engine (Multi-threaded, 8 cores)

| Mode | Resolution | Avg FPS | Throughput |
| :--- | :--- | :--- | :--- |
| 64-bit scalar (no SIMD) | 1920×1080 | ~30 fps | ~62 Mpx/s |
| 64-bit AVX2 (4× SIMD) | 1920×1080 | ~115 fps | ~239 Mpx/s |
| 128-bit `simd-f128` (AVX2 double-double) | 1920×1080 | ~16 fps | ~33 Mpx/s |
| 64-bit AVX2 | 3840×2160 (4K) | ~30 fps | ~249 Mpx/s |

> [!NOTE]
> 128-bit mode uses software-emulated double-double arithmetic via AVX2. The ~7× slowdown versus 64-bit is expected and still significantly faster than a naive `__float128` implementation (~20–30× slower).

### GPU Engine (Sokol GL / OpenGL 3.3)

| Mode | Resolution | Avg FPS | Throughput |
| :--- | :--- | :--- | :--- |
| 32-bit shader (native float) | 1920×1080 | ~79 fps | ~163 Mpx/s |
| 64-bit emulation (Hi-Lo Dekker) | 1920×1080 | ~60 fps | ~124 Mpx/s |

> [!TIP]
> To reproduce these numbers, build with `-DBUILD_CPU=ON` or `-DBUILD_GPU=ON` and run the benchmarks in `benchmarks/cpu/` or `benchmarks/gpu/` respectively. See [Benchmark Methodology](#benchmark-methodology) for the full test setup.

---

## Benchmark Methodology

All CPU benchmarks in `benchmarks/cpu/` are run using `CLOCK_MONOTONIC` for nanosecond-resolution wall-clock timing. The standard test grid is **1024×1024 pixels** at **1000 max iterations**, centered at the default Mandelbrot view. GPU benchmarks in `benchmarks/gpu/` measure frame time via `sokol_time`.

**Reproducibility checklist:**

1. Build with the appropriate CMake target and `Release` build type:
   ```bash
   cmake -S . -B build_bench -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
   cmake --build build_bench
   ./build_bench/benchmark_math
   ./build_bench/benchmark_renderer
   ```
2. Close background applications to minimize OS scheduling noise.
3. Ensure the CPU is not thermally throttled — run a warm-up pass before recording.
4. The benchmark suite runs each kernel sequentially (scalar → AVX2 → AVX-512 → simd-f128) and prints time in seconds and throughput in Mpx/s.

Benchmark results are also automatically reported as **GitHub Step Summary** in all CI pipelines (Linux, macOS, Windows) on every push.

---

## Notable Zoom Coordinates

The following coordinates are hand-picked across all 6 fractal types and used as tour waypoints in `tour.c`. They are also excellent starting points for manual exploration. Paste them into the Settings panel or use URL deep-links.

### Mandelbrot Set

| Name | Re | Im | Character |
| :--- | :--- | :--- | :--- |
| Sea Horse Valley | `-0.743643887074537` | `0.131825904145753` | Dense spiral tendrils, classic deep zoom target |
| Elephant Valley | `0.275275641098809` | `0.006942671571179` | Repeating elephant-trunk spirals |
| Triple Spiral | `-0.162736800339303` | `0.878583137739572` | Triple-arm Misiurewicz point |
| Quad Spiral | `-0.458345355141416` | `-0.633156886463435` | Four-fold spiral junction |
| Lightning | `-0.761574` | `-0.0848` | High-contrast branching filaments |
| Antenna Tip | `-1.768778833` | `-0.001738996` | Miniature full-set copy near the antenna |
| Siegel Disk | `0.001643721` | `0.822467633` | Smooth quasiperiodic Siegel disk boundary |

### Julia Set (c-parameter keyframes)

| c (Re, Im) | Character |
| :--- | :--- |
| `(-0.7000, +0.2700)` | Classic connected Julia — smooth spirals |
| `(-0.4000, +0.6000)` | Dendrite structure |
| `(+0.2850, +0.0100)` | Cauliflower — near the real axis bulb |
| `(-0.7269, +0.1889)` | Douady rabbit — three-period bulb |
| `(-0.8000, +0.1560)` | Siegel dragon |
| `(-0.1200, -0.7700)` | San Marco fractal |
| `(-0.6180, +0.0000)` | Golden ratio Julia — self-similar spirals |

### Burning Ship

| Re | Im | Character |
| :--- | :--- | :--- |
| `-1.7550` | `-0.0300` | Ship prow region — flame-like structures |
| `-1.8600` | `-0.0050` | Mini-ship copy — complete self-similar replica |
| `-1.7455` | `-0.0400` | Mast and rigging — fine linear features |

> [!TIP]
> Use the URL deep-link format to share any view directly: `?re=<Re>&im=<Im>&z=<zoom>&it=<iterations>&p=<palette>&f=<fractal>`

---

## Web vs Desktop Feature Matrix

Some features are only available on the native desktop build due to platform restrictions (filesystem access, OpenGL context requirements, or browser security policies).

| Feature | Desktop CPU | Desktop GPU | Web (Browser) |
| :--- | :---: | :---: | :---: |
| Mandelbrot / Julia / Burning Ship | Yes | Yes | Yes |
| Tricorn / Celtic / Buffalo | Yes | Yes | Yes |
| 22 color palettes | Yes | Yes | Yes |
| 14 GPU-exclusive shader palettes | - | Yes | Yes |
| AVX2 / AVX-512 SIMD | Yes | - | - |
| ARM NEON SIMD | Yes | - | - |
| WASM-SIMD128 | - | - | Yes |
| Multi-threaded rendering | Yes | - | Yes (Web Workers) |
| 128-bit precision (`simd-f128`) | Yes | - | - |
| GPU Hi-Lo Dekker 64-bit | - | Yes | Yes |
| Perturbation Theory + SA | Yes | Yes | - |
| Tour mode (all 6 fractal types) | Yes | Yes | Yes (Mandelbrot + Julia circular) |
| Screenshot (PNG) | Yes | Yes | Yes |
| Mega Screenshot (8K TGA) | Yes | - | - |
| Bookmark save/load (JSON) | Yes | Yes | - |
| URL deep-link sharing | - | - | Yes |
| Video Studio (MP4 export) | Yes | - | - |
| Touch / pinch-to-zoom | - | - | Yes |
| Settings panel | Yes | Yes | Yes |
| About panel | - | - | Yes |

> [!NOTE]
> The Web build uses a **Service Worker** (`coi-serviceworker.js`) to inject `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers, enabling `SharedArrayBuffer` for true multi-threading without requiring special server configuration.

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
cmake -S . -B build_cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_cpu
./build_cpu/mandelbrot_cpu

# Desktop — GPU engine
cmake -S . -B build_gpu -DBUILD_GPU=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_gpu
./build_gpu/mandelbrot_gpu

# Web (WASM)
emcmake cmake -S . -B build_web -DBUILD_WEB=ON
cmake --build build_web
# Output is automatically copied to the deploy/ folder
```

### Running the Web Build Locally

The web build requires specific HTTP security headers (`COOP`/`COEP`) to enable `SharedArrayBuffer`. Use the included server script:

```bash
python3 scripts/server.py
```

Then open `http://localhost:8081` in your browser.

Optional arguments:

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--port` | `8081` | Port to listen on |
| `--dir` | `web` | Directory to serve |

```bash
# Example: serve the deploy/ folder on port 9000
python3 scripts/server.py --dir deploy --port 9000
```

---

## Configuration

Rendering parameters can be tuned in `include/config.h`:

| Parameter | Default | Description |
| :--- | :--- | :--- |
| `WINDOW_WIDTH` / `WINDOW_HEIGHT` | `1024` / `768` | Initial window resolution |
| `DEFAULT_ITERATIONS` | `1200` (Desktop) / `350` (Web) | Initial iteration depth — lower on Web for browser responsiveness |
| `MAX_ITERATIONS_LIMIT` | `10000` | Upper bound for runtime adjustments |
| `DEFAULT_THREAD_COUNT` | `0` (auto-detect) | Number of parallel threads (`0` = detect from CPU cores, max 64) |
| `ESCAPE_RADIUS` | `10.0` | Squared-distance divergence threshold |
| `DEFAULT_PALETTE` | `0` | Starting color palette index (see table below) |
| `INITIAL_CENTER_RE` / `INITIAL_CENTER_IM` | `-0.5` / `0.0` | Initial view center (complex plane) |
| `INITIAL_ZOOM` | `3.0` | Initial zoom level |
| `MAX_HISTORY_SIZE` | `100` | Maximum undo history depth |
| `PERTURBATION_ZOOM_THRESHOLD` | `1e-6` | Zoom level below which Perturbation Theory activates automatically |

### Color Palettes

The engine ships with **22 built-in palettes**, selectable at runtime with `P` or the settings panel, or set as default via `DEFAULT_PALETTE` in `config.h`. Palettes marked *(GPU)* use dedicated GLSL shader effects and fall back to a CPU-compatible gradient on CPU mode:

| Index | Name | Character |
| :--- | :--- | :--- |
| `0` | Sine Wave | Smooth cycling colors using sine-wave interpolation |
| `1` | Volumetric Magma | Realistic blackbody radiation curve |
| `2` | Viridis | Perceptually uniform blue-to-yellow gradient |
| `3` | Grayscale | Pure luminance, iteration count mapped to brightness |
| `4` | Electric | Red-dominant, high-contrast neon feel |
| `5` | Ocean | Multi-frequency blue-green depth gradient |
| `6` | Inferno | Deep blue-to-white, high-zoom detail emphasis |
| `7` | Retro Binary | Alternating green/blue pixel art aesthetic |
| `8` | Orbit Mesh *(GPU)* | Cyan-to-navy orbit trap mesh overlay |
| `9` | Biomorph Trap *(GPU)* | Gold-to-dark-blue biomorphic trap coloring |
| `10` | Conformal Ripples *(GPU)* | Concentric ripple wave generator |
| `11` | Curvature Marble *(GPU)* | Psychedelic multi-frequency wave rings |
| `12` | Conformal Grid *(GPU)* | Deep navy-to-bright-cyan conformal grid |
| `13` | Cyber Grid *(GPU)* | Neon cyan-to-dark-green grid lines |
| `14` | Bubble Pearl *(GPU)* | Teal-to-copper iridescent pearl blend |
| `15` | Liquid Chrome *(GPU)* | Iridescent silver reflection wave cycles |
| `16` | Refractive 3D Glass *(GPU)* | Ice-blue to light-purple wave gradient |
| `17` | Ultra Fractal Classic *(GPU)* | Multi-stop color wheel cycling |
| `18` | Pure Binary BW | Hard black-and-white iteration boundary |
| `19` | Classic Royal Blue *(GPU)* | Deep navy-to-cyan-to-white royal gradient |
| `20` | Classic Fire Red *(GPU)* | Black-to-red-to-orange-to-white fire ramp |
| `21` | Silver Crimson *(GPU)* | Silver shimmer with deep-zoom crimson edge bloom |

All palettes use fractional iteration interpolation to eliminate color banding at region boundaries.

---

## Running Tests

The test suite covers core mathematical correctness, AVX2 vs scalar consistency, threading correctness, and I/O validation. Tests are integrated into the CMake build system and run via `ctest`.

```bash
cmake -S . -B build_cpu -DBUILD_CPU=ON
cmake --build build_cpu
ctest --test-dir build_cpu --output-on-failure
```

| Test | Description |
| :--- | :--- |
| `test_math` | Verifies Mandelbrot/Julia/Burning Ship escape math, cardioid/period-2 bulb rejection, and AVX2 vs scalar consistency within `1e-7` |
| `test_simd_math` | Dedicated SIMD path correctness tests — verifies AVX2/AVX-512/NEON outputs match scalar reference across all 6 fractal types |
| `test_renderer` | Validates the persistent thread pool dispatch — ensures pixel output is correctly produced across all worker threads |
| `test_color` | Confirms all 22 palette functions produce valid ARGB values and gradient continuity |
| `test_bookmark` | Tests bookmark serialization and round-trip load/save correctness |
| `test_tour` | Validates tour phase state machine transitions and coordinate interpolation |
| `test_config` | Verifies `settings.txt` parsing and default fallback values |
| `test_app_state` | Validates app state serialization, restoration, and cross-backend consistency |
| `test_perturbation` | Verifies perturbation theory math consistency against the reference scalar path |
| `test_bignum` | Validates the software arbitrary-precision BigNum engine — addition, multiplication, and division correctness |

> AVX2/AVX-512 tests are compiled and run automatically if the host CPU supports them. On machines without SIMD support, the scalar path is used and consistency tests are skipped.

---

## Project Structure

```text
.
├── include/             # Global configuration and platform headers
│   ├── config.h        # Tunable rendering parameters
│   ├── core/           # Core math type headers (fractal.h, mandelbrot.h, etc.)
│   ├── engine/         # Engine interface headers (renderer.h, tour.h, etc.)
│   ├── ui/             # HUD and UI layer headers
│   └── app/            # Application-layer headers (wasm_bridge.h)
├── src/
│   ├── core/           # Pure Mathematical Engine (Single Source of Truth)
│   │   ├── fractal.c   # Central fractal registry and SIMD dispatch
│   │   ├── mandelbrot.c, julia.c, burning_ship.c
│   │   ├── tricorn.c, celtic.c, buffalo.c
│   │   ├── color.c     # 22 palettes with LUT generation
│   │   ├── bignum.c    # Software arbitrary-precision integer engine
│   │   └── mandelbrot_bignum.c  # BigNum Mandelbrot iteration (research)
│   ├── engine/         # Platform-Agnostic Logic
│   │   ├── renderer.c  # Multi-threaded CPU renderer + thread pool
│   │   ├── app_state.c # Shared application state machine
│   │   ├── tour.c      # Automated camera path system
│   │   ├── bookmark.c  # JSON bookmark save/load
│   │   ├── screenshot.c # Deferred GPU readback + PNG/TGA export
│   │   ├── perturbation.c # Reference-orbit engine for deep zoom
│   │   ├── camera.c    # View transform and zoom history
│   │   ├── config_loader.c # settings.json / settings.txt persistent config
│   │   └── input_handler.c # Unified keyboard/mouse dispatch
│   ├── ui/             # HUD & UI Rendering Layer
│   │   ├── hud_sdl.c   # HUD for CPU desktop (SDL2)
│   │   ├── hud_sokol.c # HUD for GPU desktop and Web (Sokol/Fontstash)
│   │   ├── hud_settings.c  # Settings panel UI
│   │   ├── hud_video_studio.c # Video Studio GUI (ImGui-based)
│   │   └── imgui_impl.cc   # Dear ImGui backend for Video Studio
│   └── app/            # Platform-Specific Entry Points (thin wrappers)
│       ├── app_runner.c        # Unified Sokol app runner
│       ├── desktop_cpu_main.c
│       ├── desktop_gpu_main.c
│       ├── web_main.c
│       ├── wasm_bridge.c       # JS↔WASM function exports
│       └── video_renderer_main.c
├── shaders/             # GLSL shader source files (sokol-shdc format)
├── web/                 # Web Frontend
│   ├── index.html, app.js, style.css
│   └── coi-serviceworker.js   # COOP/COEP header injection for SharedArrayBuffer
├── assets/              # Shared fonts and media
├── tests/               # Automated Unit Tests (ctest)
├── benchmarks/
│   ├── cpu/            # CPU benchmarks (math kernels, renderer, I/O)
│   └── gpu/            # GPU benchmarks (Sokol shader throughput)
├── third_party/         # Vendored external libraries
│   ├── sokol/          # Sokol headers (GFX, App, GL, Time)
│   ├── stb/            # stb_image_write for PNG/TGA export
│   ├── fons/           # Fontstash for HUD text rendering
│   ├── simd-f128/      # AVX2-accelerated 128-bit double-double precision
│   ├── cimgui/         # Dear ImGui C bindings (Video Studio GUI)
│   └── cjsonx/         # Lightweight JSON parser (bookmark I/O)
├── scripts/             # Utility scripts
│   └── server.py       # Local dev server with COOP/COEP headers
├── deploy/              # Generated by web build — ready-to-serve
├── CMakeLists.txt       # Unified Cross-platform Build System
├── build.sh             # Interactive TUI Build Wrapper
├── CHANGELOG.md         # Full history of changes
└── CONTRIBUTING.md      # Contribution guide
```

---

## Roadmap

### Performance Optimization

- [x] Implement dynamic load balancing using atomic row-counters to maximize CPU utilization.
- [x] Integrate a pre-calculated Look-Up Table (LUT) for color mapping.
- [x] Implement smooth coloring algorithms using fractional iteration counts.
- [x] Deploy hardware-specific vectorization (AVX2, AVX-512, WASM-SIMD128, ARM NEON).
- [x] Research and implement pure-shader fractal calculation for GPU rendering.
- [x] Optimize all 6 fractal types with hardware-specific SIMD vectorization.
- [x] Implement Perturbation Theory with Series Approximation for deep-zoom rendering past $10^{14}$.

### Features and Exploration

- [x] Add interactive runtime controls for iteration depth and palette switching.
- [x] Implement automated "camera path" and "tour" modes.
- [x] Connect HTML5 Frontend APIs to the web-engine for a responsive experience.
- [x] Implement URL-based state recovery and deep-linking for sharing discoveries.
- [x] Add mobile touch support (pinch-to-zoom and gesture-based panning).
- [x] Implement 6 fractal types via a pluggable central registry (Mandelbrot, Julia, Burning Ship, Tricorn, Celtic, Buffalo).
- [x] Implement 22 color palettes including 14 GPU-exclusive shader effects.
- [x] Build Mandelbrot Video Studio with GUI and headless CLI rendering.
- [x] Implement bookmark system with JSON persistence for saving and restoring coordinates.

### Engineering and Quality

- [x] Establish a strict Engine-Centric Monorepo architecture.
- [x] Implement a high-performance CMake build system.
- [x] Expand unit testing coverage (math, renderer, color, bookmark, tour, config, app_state, perturbation).
- [x] Implement automatic CPU core detection for dynamic thread pool allocation.
- [x] Implement Hi-Lo 64-bit precision emulation for GPU shaders.
- [x] Implement 128-bit software double-double precision via `simd-f128` (AVX2-accelerated) for deep CPU zoom.
- [x] Build a comprehensive benchmark suite covering CPU math kernels, multi-threaded renderer throughput (64-bit and 128-bit), image I/O, and GPU shader throughput.
- [x] Integrate automated performance benchmarks into all CI pipelines (Linux, macOS, Windows) with GitHub Step Summary reports.
- [x] Add Enterprise CI workflows: Code Formatter Enforcement (`clang-format`), Memory Safety (`Valgrind`), and Static Security Analysis (`CodeQL`).
- [x] Implement Perturbation Theory reference-orbit engine and Series Approximation skip.
- [x] Complete Perturbation Theory integration into the live GPU renderer — grid-search reference selection, Dekker-split orbit texture upload, and automatic activation below `PERTURBATION_ZOOM_THRESHOLD`.
- [x] Research and implement arbitrary-precision arithmetic for infinite zoom — `bignum.c` software BigNum engine, `mandelbrot_bignum.c` iteration kernel, and `perturbation_compute_bignum()` reference orbit integrated into the live renderer. Activates automatically below `BIGNUM_ZOOM_THRESHOLD` (`1e-60`).

---

## Contributing

Contributions, bug reports, and suggestions are welcome. Areas of particular interest include memory safety, SIMD optimization, and GPGPU improvements.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full guide, including build instructions, test procedures, and code style requirements.

To contribute:

1. Open an **issue** to discuss bugs or proposed changes.
2. **Fork** the repository and open a **pull request** with your changes.
3. Descriptive commit messages and clear explanations are appreciated.

See [CHANGELOG.md](CHANGELOG.md) for a full history of changes.

---

## Development Methodology & AI Assistance

Building a high-performance fractal engine in C involves navigating complex engineering tradeoffs — from SIMD vectorization strategies and IEEE 754 floating-point precision limits, to lock-free thread pool design and cross-platform shader compatibility.

To achieve this level of stability and performance, this project was architected and rigorously verified in collaboration with **Advanced Agentic AI**. AI was specifically utilized to:

- Validate AVX2 intrinsic correctness and ensure scalar/SIMD result consistency within `1e-7` tolerance.
- Assist in designing the persistent thread pool architecture (condition variable signalling, atomic row counter load balancing).
- Verify Hi-Lo double-single arithmetic in GLSL shaders for 64-bit precision emulation without hardware double support.
- Automate the generation of robust cross-platform CI/CD pipelines (Linux, macOS, Windows, WASM) including memory safety checks and static analysis.

However, **human agency remains at the core of this project**. Every line of code generated or suggested was manually inspected, audited, and verified. The core architecture, algorithms, and mathematical implementation were human-planned. This hybrid approach — combining human architectural vision with AI-driven debugging and verification — allowed this project to reach a level of engineering quality well beyond what a solo developer could achieve alone.

---

## Author's Note

I'm just a kid building projects as a hobby. Thank you for showing interest in my little library! It really means a lot to me. :)

---

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
