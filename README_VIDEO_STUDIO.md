# Mandelbrot Video Studio Guide

The **Mandelbrot Video Studio** (`mandelbrot_video`) is a high-performance video exporting tool that compiles fractal zoom animations into high-quality `.mp4` videos using a multi-threaded CPU rendering engine and FFmpeg.

It supports two modes of operation:
1. **GUI Mode**: An interactive, centered settings studio panel.
2. **Headless CLI Mode**: A command-line interface to render videos directly in the terminal without opening any graphical windows.

---

## 1. Quick Start

### Build the Video Studio
To compile the video renderer:
```bash
./build.sh
# Select option 5 (mandelbrot_video)
```
Or build manually:
```bash
cd build_video
make mandelbrot_video
```

### Launch GUI Mode
To run the interactive GUI:
```bash
./build_video/mandelbrot_video
```
*(Passing CLI arguments here will pre-fill their corresponding fields inside the GUI.)*

### Run Headless CLI Render
To render a 5-second video at 1080p, 60fps headlessly:
```bash
./build_video/mandelbrot_video --headless --width 1920 --height 1080 --duration 5 --out output.mp4
```

---

## 2. Command Line Options

Run with `-h` or `--help` to view all options:
```bash
./build_video/mandelbrot_video --help
```

### General Options
* `-h, --help`: Show help text and exit.
* `--headless`: Bypasses window creation and renders the video fully in the command line.

### Video Options
* `-w, --width <pixels>`: Width of the output video (default: `1280`).
* `-g, --height <pixels>`: Height of the output video (default: `720`).
* `-f, --fps <fps>`: Video frame rate (default: `60`).
* `-d, --duration <seconds>`: Video duration in seconds (default: `10`).
* `-o, --out <filename>`: Output filename or save path (default: `mandelbrot_video.mp4`).

### Encoder Options
* `-c, --crf <0-51>`: Constant Rate Factor for quality. `0` is lossless, `18` is high, `23` is medium, `28` is low (default: `18`).
* `-s, --preset <speed>`: FFmpeg preset: `ultrafast`, `superfast`, `veryfast`, `faster`, `fast`, `medium`, `slow`, `slower`, `veryslow` (default: `fast`).
* `--codec <h264|h265>`: Output encoder format (default: `h264`).
* `--aa <1|2|4>`: Supersampling anti-aliasing level (default: `1` = no anti-aliasing).

| AA Level | Description | Render Cost |
| :--- | :--- | :--- |
| `1` | No anti-aliasing (fastest) | 1× |
| `2` | 2×2 supersampling — smooths jagged edges | 4× |
| `4` | 4×4 supersampling — cinema-quality output | 16× |

### Animation Options
* `-p, --path <scenic|bookmarks|custom>`: Animation path type:
  * `scenic`: Animates along built-in preset points.
  * `bookmarks`: Animates sequentially between your saved bookmarks.
  * `custom`: Zooms directly to a specific target coordinate set in the GUI settings panel or via `settings.txt`.
* `--curve <0-3>`: Camera movement interpolation ease curve:
  * `0`: Ease In/Out (Smoothstep) (default)
  * `1`: Linear (Constant speed)
  * `2`: Ease In (Quadratic)
  * `3`: Ease Out (Quadratic)

### Telemetry Overlay (Log) Options
* `--log`: Enable real-time telemetry overlay on the video.
* `--log-size <size>`: Font size for the log overlay text (default: `20`).
* `--log-font <path>`: Path to a custom TrueType `.ttf` font file (default: `assets/fonts/font.ttf`).
* `--log-pos <0-3>`: Overlay placement:
  * `0`: Top Left (default)
  * `1`: Top Right
  * `2`: Bottom Left
  * `3`: Bottom Right
* `--log-opacity <opacity> `: Background box transparency (default: `0.6`).
* `--log-color <color>`: Text color (e.g. `white`, `yellow`, `cyan`, `green`, `red`, `magenta`, `orange`) (default: `white`).

---

## 3. Usage Examples

### A. 4K High-Quality Scenic Tour
Renders a 4K video at 60fps using high quality settings and a smooth zoom animation curve:
```bash
./build_video/mandelbrot_video --headless -w 3840 -g 2160 -f 60 -d 15 -c 16 -s slow --curve 0 --out 4k_scenic.mp4
```

### B. Custom Zoom with Yellow Overlay Log in Top Right
Zooms to the custom target specified in settings, displaying a yellow overlay log in the top-right corner with 80% box opacity:
```bash
./build_video/mandelbrot_video --headless -p custom -d 8 --log --log-pos 1 --log-color yellow --log-opacity 0.8 --out custom_zoom_log.mp4
```

### C. Fast Draft Verification
Generates a fast, low-resolution preview video:
```bash
./build_video/mandelbrot_video --headless -w 640 -g 480 -f 30 -d 3 -c 28 -s ultrafast --out draft.mp4
```

### D. High-Quality with Anti-Aliasing
Renders a smooth 1080p video with 2× supersampling for cleaner edges:
```bash
./build_video/mandelbrot_video --headless -w 1920 -g 1080 -f 60 -d 10 --aa 2 -c 18 -s slow --out hq_aa.mp4
```

---

## 4. Tips

> [!TIP]
> **Workflow recommendation:** Run a fast draft first (`-w 640 -g 480 -s ultrafast`) to verify the animation path and timing, then re-render at full quality.

> [!TIP]
> **Codec choice:** Use `h264` (default) for maximum compatibility. Switch to `h265` for smaller file sizes when uploading to platforms that support it (YouTube, Vimeo).

> [!NOTE]
> **Anti-aliasing cost:** AA level `4` renders at 4× the resolution internally before downsampling. On an 8-core machine, a 10-second 1080p render at AA=4 can take several minutes. Use `--aa 2` as a good balance between quality and speed.
