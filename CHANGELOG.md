# Changelog

All notable changes to Mandelbrot-C are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased]

### Added

- `about ℹ` button in the web UI — explains the project and web vs desktop feature differences
- `F` / `B` keyboard shortcut in the web UI now correctly triggers fractal mode cycling
- URL deep-link now persists `base_fractal` mode (`?f=` parameter), so shared links restore the correct fractal type

---

## [v5.1.0] — 2026-06-25

### Fixed

- GPU app: pass camera center coordinates instead of `re_min`/`im_bot` to the renderer, correcting a coordinate transformation bug at non-default view positions

---

## [v5.0.2] — 2026-07-08

### Fixed

- **Web bookmarks now restore fractal type** — Shared links (Ctrl+Alt+C) now include the `base_fractal` parameter, ensuring the correct fractal type (e.g., Julia, Burning Ship) is restored when the bookmark URL is opened
- **`F` and `B` keys now toggle fractal mode in Web UI** — The `f` and `b` keyboard shortcuts now correctly cycle through Mandelbrot, Julia, Burning Ship, Tricorn, Celtic, and Buffalo modes in the browser, matching desktop behavior

---

## [v5.0.1] — 2026-06-22

### Fixed

- CI: use `7z` instead of `zip` for packaging release binaries on Windows (resolves build pipeline failure)

---

## [v5.0.0] — 2026-06-22 — Engine-Centric Monorepo

### Changed — Architecture

- **Full Engine-Centric Refactor:** Entire codebase reorganized into a strict 3-layer architecture:
  - `src/core/` — Pure mathematical engine (SSOT, no platform dependencies)
  - `src/engine/` — Platform-agnostic renderers, tour, bookmark, screenshot, perturbation, app state
  - `src/app/` — Platform-specific thin entry points (one file per target, no `#ifdef` sprawl)
- **Unified Sokol App Runner:** All desktop and web targets share a single `app_runner.c` dispatched by `APP_BACKEND_*` enum — eliminates duplicated init/teardown code
- **Static library build:** CMake now builds `mandelbrot_lib` as a shared static library; all targets (tests, benchmarks, apps) link against it
- **Automatic shader embedding:** `sokol-shdc` shader compilation integrated into CMake; shaders are embedded as C headers at build time

### Added — Fractal Types

- **Tricorn** (`tricorn.c`) — conjugate Mandelbrot: $\bar{z}^2 + c$, with scalar, AVX2, AVX-512, WASM-SIMD128, and ARM NEON paths
- **Celtic** (`celtic.c`) — Celtic variant: $|\text{Re}(z^2)| + i\,\text{Im}(z^2) + c$, same SIMD coverage
- **Buffalo** (`buffalo.c`) — Buffalo variant: $|z|^2 + c$, same SIMD coverage
- **Fractal Registry** (`fractal.c`) — central pluggable registry dispatching math kernels at runtime; up to 16 fractal types supported; all share the same SIMD infrastructure

### Added — ARM NEON

- ARM NEON (64-bit) vectorized paths for Mandelbrot, Julia, Burning Ship, Tricorn, Celtic, Buffalo — enables native performance on Apple Silicon and ARM64 Linux
- `check_neon` function pointer added to `FractalDefinition`

### Added — Rendering

- **All 6 fractal types in GPU shader** — `base_fractal_mode` uniform switches between formulas with no pipeline rebuild
- **All 22 palettes in GPU shader** — GPU-exclusive palettes use orbit traps, conformal mappings, and domain coloring techniques
- Perturbation Theory **fully integrated** into the live GPU renderer — automatic grid-search (11×11) finds the best reference coordinate each frame, Dekker-split reference orbit uploaded as GPU texture, activates automatically when `zoom < PERTURBATION_ZOOM_THRESHOLD` on Mandelbrot GPU mode
- `PERTURBATION_ZOOM_THRESHOLD` constant in `config.h`

### Added — Engine

- **Unified Input Handler** (`input_handler.c`) — single authoritative keyboard/mouse dispatch shared by all desktop and web backends
- **Stacked Notification System** in `app_state.c` — HUD messages queue and display without overwriting each other
- **`app_state_cycle_fractal()`** — cycles through all registered fractal types in order
- **Async Mega-Screenshot** — 8K tiled render runs in a dedicated thread with real-time HUD progress reporting
- **HUD modules split:** `hud_sdl.c` (CPU desktop) and `hud_sokol.c` (GPU desktop/web) — separated from main render loop
- `desktop_deep_main.c` — dedicated entry point for the perturbation/deep-zoom build target

### Added — Tests & Benchmarks

- `test_perturbation` — verifies reference orbit and SA coefficient math
- `test_app_state` — validates state serialization, restoration, and cross-backend consistency
- Thread scaling sweep benchmark added to `benchmark_renderer.c`
- AVX-512 math benchmark added to `benchmark_math.c`

### Added — Web Frontend

- Tricorn, Celtic, and Buffalo added to the web fractal mode selector
- Web UI updated to use unified input handler and `base_fractal` enum
- Settings panel dynamically populated from WASM fractal registry at runtime

---

## [v4.2.0] — 2026-04-29 — Hi-Lo GPU Precision & State Sync

### Added

- **Hi-Lo 64-bit GPU Precision** (Dekker double-single arithmetic) — `center_hi` / `center_lo` uniform pair achieves ~48-bit mantissa precision in the fragment shader, enabling deep zoom without pixelation on GPU
- Runtime toggle `E` between 32-bit and 64-bit GPU precision (alert shown on web if 128-bit CPU precision is attempted)
- Authoritative state sync: `updateDebugInfo()` JS callback now pushes GPU state back to frontend UI every frame — eliminates desync between C engine and web buttons
- `guard key E` in desktop engine for consistency with web precision toggle behavior

---

## [v4.1.1] — 2026-04-27 — Web Shader Fix

### Fixed

- Web shader uniform alignment and size mismatch — resolves rendering glitches and GPU pipeline errors in WebGL 2.0
- Color mismatch between GPU and CPU palette rendering paths corrected

---

## [v4.1.0] — 2026-04-24 — UI Theme Restoration

### Changed

- Original monospace "mint" terminal theme restored for CPU, GPU, and Web builds after theme regression
- HUD styling consistency pass across all three build targets

---

## [v4.0.0] — 2026-04-24 — GPU Engine & Full Feature Expansion

### Added — GPU Rendering

- **Sokol GFX integration** — full GPU rendering pipeline via OpenGL 3.3 (Desktop) and WebGL 2.0 (Web)
- Fragment shader implements Mandelbrot + Julia set math in GLSL
- Cardioid and period-2 bulb rejection in shader (same as CPU path)
- `julia_mode` uniform switches between Mandelbrot and Julia modes per frame
- Escape radius matches `config.h` (`ESCAPE_RADIUS = 10.0`) on both CPU and GPU

### Added — Precision

- **128-bit software double-double precision** via `simd-f128` (AVX2-accelerated) for deep CPU zoom — ~7× slower than 64-bit AVX2, still significantly faster than `__float128`
- Precision toggle `E`: CPU switches between 64-bit and 128-bit; GPU switches between 32-bit and 64-bit emulation

### Added — Perturbation Theory (Research)

- `perturbation.c` — reference orbit engine computing center orbit at double precision
- Series Approximation (SA) coefficients A, B, C computed alongside orbit for skipping early iterations
- Perturbation rendering path integrated into GPU shader as opt-in mode
- `mandelbrot_deep` CMake target added for perturbation-focused builds

### Added — Screenshot & Video

- **Professional Screenshot System** — deferred GPU readback ensures correct frame capture
- `stb_image_write` PNG export with ARGB→RGBA conversion on desktop
- **Async Mega-Screenshot** — tiled 8K TGA export in background thread

### Added — Web

- WebAssembly build target via Emscripten
- WASM-SIMD128 vectorization (2× double-precision throughput in browser)
- `SharedArrayBuffer` multi-threading via Emscripten pthreads + Web Workers
- `coi-serviceworker.js` — injects COOP/COEP headers on static hosting
- Touch support: pinch-to-zoom and two-finger pan
- URL state sharing: `?re=`, `?im=`, `?z=`, `?it=`, `?p=`, `?j=`, `?jre=`, `?jim=`

### Added — CI/CD

- GitHub Actions workflows: Linux, macOS, Windows, WASM
- **CodeQL** static security analysis
- **Valgrind** memory safety (`memcheck` workflow)
- **clang-format** enforcement workflow
- Automated benchmark reports in GitHub Step Summary
- Release workflow: builds all platforms and publishes to GitHub Releases on `v*` tags

---

## [v3.0.0] — 2026-04-14 — WebAssembly Frontend

### Added

- Full web frontend: `index.html`, `app.js`, `style.css`
- WebAssembly main loop (`web_main.c`) integrated with Emscripten event loop
- Web UI buttons: Reset, Undo, Tour, Palette, GPU toggle, Screenshot, Copy Link, Settings
- Settings panel with coordinate inputs, Julia controls, and mode selectors
- `coi-serviceworker.js` for COOP/COEP injection
- `scripts/server.py` — local dev server with required security headers
- Julia mode pointer-tracking lock (`toggleJuliaLock`)
- URL deep-link generation and restoration on page load

---

## [v2.3.1] — 2026-04-05

### Changed

- README improved with CI badges and Table of Contents

---

## [v2.3.0] — 2026-04-05 — Bookmark System

### Added

- **Bookmark system** — save (`M`) and load (`L`) named coordinate positions, persisted to `bookmarks.json`
- **Undo history** — up to 100 levels of zoom/pan undo (`Ctrl+Z`), stored in camera history ring buffer
- **Julia Set** live exploration with interactive c-parameter control
- **Burning Ship** fractal variant toggle (`B`)
- Palette cycle key `P`
- Settings loader (`config_loader.c`) reads `settings.txt` for persistent configuration
- `test_bookmark` and `test_config` added to test suite

---

## [v2.2.0] — 2026-03-31 — AVX2 SIMD

### Added

- **AVX2 vectorization** — 256-bit YMM registers process 4 double-precision complex numbers per cycle (4× scalar throughput)
- AVX-512 support on capable hardware (8× scalar throughput)
- `test_math` extended with AVX2 vs scalar consistency checks within `1e-7` tolerance
- SIMD paths skip cardioid/bulb detection to maintain vectorized throughput

---

## [v2.1.0] — 2026-03-28 — Tour Mode

### Added

- **Interactive Tour Mode** (`T`) — automated camera path with smoothstep easing
- Mandelbrot tour: 10 hand-picked deep-zoom coordinates, 3-phase sequence (Pan 1.8s, Zoom In 4.0s, Zoom Out 3.2s), random target selection without repeat
- Julia tour (desktop): 12 preset c-parameter keyframes (3.0s move, 1.2s dwell)
- `test_tour` added to test suite

---

## [v2.0.0] — 2026-03-22 — Multi-threaded Engine

### Added

- **Persistent Thread Pool** — threads spawned once at startup, managed via condition variables; up to 64 threads (auto-detected from CPU cores)
- **Atomic Row Counter** dynamic load balancing — threads claim next available pixel row, eliminating idle cores near dense black regions
- **Julia Set** rendering support
- Color palette LUT (Look-Up Table) with fractional iteration smooth-coloring
- **Dynamic HUD** — 14-decimal precision coordinate display
- `test_renderer` and `test_color` added to test suite

---

## [v1.0.0] — 2026-03-19 — Initial Release

### Added

- Core Mandelbrot set iteration algorithm in C99
- SDL2 window management and event loop
- Single-threaded CPU rendering engine
- Main cardioid and period-2 bulb rejection optimizations
- Normalized iteration count for smooth color gradients
- CMake build configuration
- `build.sh` interactive build script
- GitHub Actions CI workflow
- `.clang-format` configuration
