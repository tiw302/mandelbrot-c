# Contributing to Mandelbrot-C

Thank you for your interest in contributing! This project welcomes bug reports, performance improvements, SIMD optimizations, and GPGPU enhancements.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Building the Project](#building-the-project)
- [Running Tests](#running-tests)
- [Code Style](#code-style)
- [Submitting Changes](#submitting-changes)
- [Areas of Interest](#areas-of-interest)

---

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/mandelbrot-c.git
   cd mandelbrot-c
   ```
3. Create a **feature branch**:
   ```bash
   git checkout -b feat/your-feature-name
   ```

---

## Project Structure

The codebase follows a strict **Engine-Centric Architecture** with clear Separation of Concerns:

```
src/
├── core/       # Pure math — no platform dependencies (SSOT)
│               #   mandelbrot.c, julia.c, burning_ship.c, tricorn.c,
│               #   celtic.c, buffalo.c, fractal.c, color.c
├── engine/     # Platform-agnostic logic
│               #   renderer.c, tour.c, bookmark.c, app_state.c,
│               #   camera.c, screenshot.c, input_handler.c, perturbation.c
└── app/        # Platform entry points (thin wrappers only)
                #   desktop_cpu_main.c, desktop_gpu_main.c,
                #   web_main.c, video_renderer_main.c
```

> **Key rule:** `src/core/` must never `#include` anything from `src/engine/` or `src/app/`.
> The core layer is the Single Source of Truth for all fractal mathematics.

---

## Building the Project

### Desktop (CPU / GPU)

```bash
# Interactive menu
./build.sh

# Or directly:
cmake -S . -B build_cpu -DBUILD_CPU=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_cpu
```

See [README.md](README.md#build-and-installation) for full prerequisites and platform-specific instructions.

### Web (WASM)

```bash
emcmake cmake -S . -B build_web -DBUILD_WEB=ON
cmake --build build_web
python3 scripts/server.py   # then open http://localhost:8081
```

---

## Running Tests

Always run the full test suite before submitting a PR:

```bash
cmake -S . -B build_cpu_test -DBUILD_CPU=ON
cmake --build build_cpu_test
ctest --test-dir build_cpu_test --output-on-failure
```

| Test | What it checks |
| :--- | :--- |
| `test_math` | Mandelbrot/Julia/Burning Ship escape math, cardioid/bulb rejection, AVX2 vs scalar within `1e-7` |
| `test_renderer` | Thread pool dispatch — pixel output across all worker threads |
| `test_color` | All 22 palette functions produce valid ARGB values and gradient continuity |
| `test_bookmark` | Bookmark serialization and round-trip load/save |
| `test_tour` | Tour phase state machine and coordinate interpolation |
| `test_config` | `settings.txt` parsing and default fallback values |
| `test_app_state` | App state serialization and restoration |
| `test_perturbation` | Perturbation theory math consistency |
| `test_bignum` | Arbitrary-precision BigNum math |

> **AVX2 note:** SIMD consistency tests are automatically skipped on machines without AVX2 support.

---

## Code Style

This project enforces formatting via **clang-format**. A `.clang-format` config file is included at the repo root.

Before committing, format all changed files:

```bash
# Format a single file
clang-format -i src/core/your_file.c

# Format all C source files
find src -name "*.c" -o -name "*.h" | xargs clang-format -i
```

The CI `Formatting` workflow will fail if any file is not properly formatted.

### General Guidelines

- **C99 only** — no C11/C23 features, no C++
- **No external runtime dependencies** in `src/core/` — pure C math only
- Keep functions focused and small — prefer extracting helpers over long functions
- AVX2/AVX512 intrinsic code must always have a scalar fallback path

### Commenting Style
This codebase enforces a specific, terse comment style:
- **Block vs Inline**: Use `/* ... */` for explanations that are long, multi-line, or describe *why* something is done. Use `//` for short, single-line notes.
- **ASCII & Decorations**: Exception: never touch `//` used for ascii art, file headers, section banners/dividers, or decorative separators — leave those exactly as-is even if long.
- **File Headers**: Every `.c` and `.h` file must start with a block comment indicating its filename, followed by a short description.
- **No Redundancy**: Do not add comments to lines that are self-explanatory from naming alone. Do not state the obvious.
- **No "AI-Sounding" Tone**: Keep comments short and to the point. Avoid overly formal phrasing like "this function is responsible for...". Flag and rewrite any comment that sounds machine-generated.
- **Tone Matching**: When unsure whether a comment is "ai-sounding", compare it against other genuinely human comments already in the same file/module and match that tone and length.

### AI Workflow Guidelines
For AI assistants (like Cursor, Gemini, Copilot) working on this repository:
- **Read These Rules First**: Always adhere to the architectural constraints and the strict commenting style above.
- **Iterative Changes**: Work file-by-file when making sweeping changes, showing a short diff-style summary before moving to the next file.
- **Minimalism**: Keep changes minimalistic and focused on the user's requirements. Do not over-engineer.

---

## Submitting Changes

1. **Open an issue first** for non-trivial changes — discuss the approach before writing code
2. Keep commits focused and atomic — one logical change per commit
3. Use descriptive commit messages:
   ```
   feat(core): add tricorn variant with SIMD vectorization
   fix(web): restore fractal mode from URL parameter on load
   docs: update palette table in README to reflect 22 palettes
   ```
4. Open a **Pull Request** against `master` with a clear description of:
   - What changed and why
   - How to test it
   - Any benchmarks if it's a performance-related change

---

## Areas of Interest

Contributions are especially welcome in these areas:

| Area | Description |
| :--- | :--- |
| **Memory safety** | Catching leaks, use-after-free, or race conditions |
| **SIMD optimization** | AVX-512, NEON (ARM), or WASM-SIMD128 improvements |
| **GPGPU** | Shader precision, new GPU-only palette effects |
| **Arbitrary precision** | Research into infinite-zoom arithmetic (see Roadmap) |
| **New fractal types** | Additional variants pluggable into the fractal registry |
| **Benchmarks** | New benchmark scenarios in `benchmarks/cpu/` or `benchmarks/gpu/` |
