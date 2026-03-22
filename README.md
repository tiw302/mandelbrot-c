# Mandelbrot C (ノ*゜▽゜*)

[![Build Status](https://github.com/tiw302/mandelbrot-c/actions/workflows/build.yml/badge.svg)](https://github.com/tiw302/mandelbrot-c/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A simple, multi-threaded Mandelbrot set explorer written in C.
This is my attempt to learn low-level graphics and thread management. It is not perfect, but I tried to make it clean and fast. (´｡• ᵕ •｡`)

---

Hi! I am currently diving into C programming and wanted to build something visual to understand pointers and memory better. This project is the result of my experiments with SDL2 and pthreads. I hope you find it interesting! ヽ(>∀<☆)ノ

![Mandelbrot Screenshot](assets/screenshot.png)
![Mandelbrot Screenshot](assets/screenshot2.png)

## The Math

The Mandelbrot set is the set of complex numbers $c$ for which the function $f_c(z) = z^2 + c$ does not diverge when iterated from $z = 0$.

In simple terms:
1. Start with $z = 0$.
2. Calculate the next value: $z_{new} = z_{old}^2 + c$.
3. Repeat. If the magnitude $|z|$ stays small forever, the point $c$ is inside the set (colored black).
4. If $|z|$ explodes (escapes to infinity), the point is outside. The color represents **how fast** it escaped.

## Features

- **Real-time Rendering:** Optimized arithmetic for smooth navigation.
- **Multi-threading:** Dynamic workload distribution across available CPU cores.
- **CPU Powered:** Pure software rendering without GPU acceleration (for educational purposes). [Read more](docs/GPU_INFO.md).
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
| **Quit** | `Esc` or `Q` |

## Configuration

Rendering parameters can be tuned in `include/config.h` to balance performance and visual fidelity:

- `MAX_ITERATIONS`: Controls the detail level of the fractal boundary.
- `THREAD_COUNT`: Number of parallel threads (set to match CPU cores).
- `ESCAPE_RADIUS`: Mathematical threshold for the set calculation.

## Contributing (o_ _)o

I am still learning, so if you spot any bugs or have suggestions for improvements (especially around memory safety!), I would really appreciate your help. Feel free to open an issue or pull request. Thank you! (⌒▽⌒)

## License

This project is licensed under the [MIT License](LICENSE) - see the [LICENSE](LICENSE) file for details.
