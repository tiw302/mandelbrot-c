# Mandelbrot C

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance, multi-threaded Mandelbrot set visualizer implementation in C using SDL2.

![Mandelbrot Screenshot](assets/screenshot.png)

This project demonstrates efficient real-time fractal rendering by leveraging CPU threading (pthreads) and low-level optimizations. It features an interactive explorer with infinite zoom capabilities and customizable rendering parameters.

## Features

- **Real-time Rendering:** Optimized arithmetic for smooth navigation.
- **Multi-threading:** dynamic workload distribution across available CPU cores.
- **Interactive Controls:** Mouse-based panning and zooming.
- **State Management:** Undo/Redo history stack for view navigation.
- **Cross-Platform:** Compatible with Linux, macOS, and Windows (via MSYS2).

## Prerequisites

- C Compiler (GCC/Clang)
- Make
- SDL2 development libraries
- SDL2_ttf development libraries

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
| **Quit** | `Esc` or `Q` |

## Configuration

Rendering parameters can be tuned in `include/config.h` to balance performance and visual fidelity:

- `MAX_ITERATIONS`: Controls the detail level of the fractal boundary.
- `THREAD_COUNT`: number of parallel threads (set to match CPU cores).
- `ESCAPE_RADIUS`: Mathematical threshold for the set calculation.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
