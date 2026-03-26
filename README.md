# Mandelbrot C (ノ*゜▽゜*)

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A simple, multi-threaded Mandelbrot and Julia set explorer written in C.
This is my attempt to learn low-level graphics and thread management. It is not perfect, but I tried to make it clean and fast. (´｡• ᵕ •｡`)

---

Hi! I am currently diving into C programming and wanted to build something visual to understand pointers and memory better. This project is the result of my experiments with SDL2 and pthreads. I hope you find it interesting! ヽ(>∀<☆)ノ

### Mandelbrot

![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot.png)
![Mandelbrot Screenshot](assets/Mandelbrot-Screenshot2.png)

### julia

![julia Screenshot](assets/julia-Screenshot.png)
![julia Screenshot](assets/julia-Screenshot2.png)

## The Math

The Mandelbrot set is the set of complex numbers $c$ for which the function $f_c(z) = z^2 + c$ does not diverge when iterated from $z = 0$.

In simple terms:
1. Start with $z = 0$.
2. Calculate the next value: $z_{new} = z_{old}^2 + c$.
3. Repeat. If the magnitude $|z|$ stays small forever, the point $c$ is inside the set (colored black).
4. If $|z|$ explodes (escapes to infinity), the point is outside. The color represents **how fast** it escaped.

### Julia Sets

A Julia set $J_c$ is the closely related fractal you get when you **fix** $c$ and let the starting point $z$ vary across the screen instead. Every single point inside the Mandelbrot set produces a different, connected Julia set. Points near the boundary of the Mandelbrot set produce the most intricate Julia sets -- which is exactly why the interactive mode is so fun to explore. (ﾉ◕ヮ◕)ﾉ*:･ﾟ✧

## Features

- **Real-time Rendering:** Optimized arithmetic for smooth navigation.
- **Multi-threading:** Dynamic workload distribution across available CPU cores.
- **Julia Set Mode:** Press `J` to instantly switch to the Julia set defined by the point under your cursor. Move the mouse to morph the fractal live.
- **Screenshot Export:** Press `S` to save the current view as a timestamped PNG -- no extra libraries needed.
- **CPU Powered:** Pure software rendering without GPU acceleration (for educational purposes). [Read more](docs/GPU_INFO.md).
- **Interactive Controls:** Mouse-based panning and zooming.
- **State Management:** Undo history stack for view navigation.
- **Cross-Platform:** Compatible with Linux, macOS, and Windows (via MSYS2).

## Prerequisites

- C Compiler (GCC/Clang)
- Make
- SDL2 development libraries
- SDL2_ttf development libraries
- zlib (almost always pre-installed; required for PNG screenshot export)

### Installation

**Debian / Ubuntu**
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev
```

**Arch Linux**
```bash
sudo pacman -S sdl2 sdl2_ttf
```

**Fedora**
```bash
sudo dnf install SDL2-devel SDL2_ttf-devel
```

**Void Linux**
```bash
sudo xbps-install -S SDL2-devel SDL2_ttf-devel
```

**macOS**
```bash
brew install sdl2 sdl2_ttf
```

**Windows (MSYS2)**
```bash
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf
```

## Build & Run

The project uses a standard Makefile for compilation.

> **Friendly tip:** If you run into build errors, please double-check that you have the [Prerequisites](#prerequisites) installed!

```bash
# Build the application
make

# Run the executable
./mandelbrot

# Clean build artifacts
make clean
```

## Usage

| Action | Control |
|--------|---------|
| **Zoom In** | Left Mouse Drag |
| **Pan** | Right Mouse Drag |
| **Undo Zoom** | `Ctrl` + `Z` |
| **Reset View** | `R` |
| **Toggle Julia Mode** | `J` |
| **Save Screenshot** | `S` |
| **Quit** | `Esc` or `Q` |

### Julia Mode

Press `J` while hovering over any point on the Mandelbrot set to jump into Julia mode.
The complex coordinate under your cursor becomes the parameter $c$ that defines the Julia set.
Move your mouse around to morph the fractal in real time -- every position gives you a completely different shape.
Press `J` again to return to the exact Mandelbrot view you left.

### Screenshots

Press `S` at any time to export the current frame as a PNG.
Files are saved in the working directory with a timestamp in the filename, e.g. `mandelbrot_20250315_142031.png`.
The encoder is built on zlib with no extra dependencies.

## Configuration

Rendering parameters can be tuned in `include/config.h` to balance performance and visual fidelity:

- `MAX_ITERATIONS`: Controls the detail level of the fractal boundary.
- `THREAD_COUNT`: Number of parallel threads (set to match CPU cores).
- `ESCAPE_RADIUS`: Mathematical threshold for the set calculation.

## Roadmap

### Performance Optimization
- [x] Implement dynamic load balancing using a work queue or tiled rendering to improve CPU utilization across cores.
- [x] Replace real-time trigonometric color calculations with a pre-calculated Look-Up Table (LUT).
- [x] Implement smooth coloring algorithms using fractional iteration counts.
- [ ] Explore SIMD (AVX/AVX2) vectorization to process multiple pixels per instruction.

### Features and Exploration
- [ ] Integrate arbitrary-precision arithmetic libraries (e.g., GNU MPFR) to support deep zooms beyond the limits of double precision.
- [ ] Add interactive controls to adjust maximum iterations and switch color palettes during runtime.
- [ ] Implement an automated "camera path" or "tour" mode for smooth zooming animations.

### Advanced Backends
- [ ] Develop a GPU-accelerated backend using OpenGL Compute Shaders or Vulkan.
- [ ] Port the project to WebAssembly using Emscripten for browser-based execution.

### Engineering Improvements
- [ ] Migrate the build system from Makefile to CMake for better cross-platform support.
- [ ] Add unit tests for core mathematical functions.
- [ ] Implement automatic CPU core detection to dynamically set the thread count.

## Contributing (o_ _)o

I am still learning, so if you spot any bugs or have suggestions for improvements (especially around memory safety!), I would really appreciate your help. Feel free to open an issue or pull request. Thank you! (⌒▽⌒)

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
