const loadingScreen = document.getElementById('loading-screen');
const statusText = document.getElementById('status');
const canvas = document.getElementById('canvas');
const debugInfo = document.getElementById('debug-info');
const zoomBox = document.getElementById('zoom-box');

const PALETTES = ["sine wave", "grayscale", "fire", "electric", "ocean", "inferno"];
let _gpuMode = true;
let _tourActive = false;
let _juliaMode = false;

function toggleTour() {
    Module._wasm_toggle_tour();
    _tourActive = !_tourActive;
    document.getElementById('tourBtn').textContent = _tourActive ? 'stop tour' : 'tour';
}

function toggleGpu() {
    Module._wasm_toggle_gpu();
    _gpuMode = !_gpuMode;
    document.getElementById('gpuBtn').textContent = _gpuMode ? 'gpu ✓' : 'cpu';
}

// Global functions called by WASM via EM_JS
window.updateDebugInfo = function(gpu_mode, julia_mode, max_iters, zoom, center_re, center_im, palette_idx, tour_phase, julia_re, julia_im) {
    let engine = julia_mode ? "julia" : "mandelbrot";
    if (gpu_mode) engine += " (gpu)";
    else engine += " (cpu)";

    let tour_str = tour_phase !== 0 ? " | tour: on" : "";

    let html = `${engine}${tour_str}\n`;

    if (julia_mode) {
        html += `c: (${julia_re.toFixed(10)}, ${julia_im.toFixed(10)})\n`;
    } else {
        html += `center: (${center_re.toFixed(10)}, ${center_im.toFixed(10)})\n`;
    }

    html += `zoom: ${zoom.toPrecision(4)} | iter: ${max_iters} | palette: ${PALETTES[palette_idx % 6]}`;

    debugInfo.textContent = html;
};

window.updateZoomBox = function(is_zooming, x, y, w, h) {
    if (is_zooming) {
        zoomBox.style.display = 'block';
        zoomBox.style.left = x + 'px';
        zoomBox.style.top = y + 'px';
        zoomBox.style.width = w + 'px';
        zoomBox.style.height = h + 'px';
    } else {
        zoomBox.style.display = 'none';
    }
};

window.downloadScreenshotData = function(ptr, w, h) {
    if (!ptr || w <= 0 || h <= 0) return;
    
    // Create a copy of the pixel data from WASM memory
    const data = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, w * h * 4);
    const pixels = new Uint8ClampedArray(data);
    const imgData = new ImageData(pixels, w, h);
    
    // Use a temporary canvas to generate the PNG
    const tmpCanvas = document.createElement('canvas');
    tmpCanvas.width = w;
    tmpCanvas.height = h;
    const ctx2d = tmpCanvas.getContext('2d');
    ctx2d.putImageData(imgData, 0, 0);
    
    const link = document.createElement('a');
    link.download = `mandelbrot_${Date.now()}.png`;
    link.href = tmpCanvas.toDataURL('image/png');
    link.click();
};

function syncSize() {
    const w = window.innerWidth;
    const h = window.innerHeight;
    if (Module._wasm_set_resolution) {
        Module._wasm_set_resolution(w, h);
    }
}

function downloadScreenshot() {
    if (Module._wasm_request_screenshot) {
        Module._wasm_request_screenshot();
    }
}

// emscripten module configuration
var Module = {
    preRun: [],
    postRun: [function() {
        syncSize();
        window.addEventListener('resize', () => {
            clearTimeout(window._rt);
            window._rt = setTimeout(syncSize, 100);
        });

        // keyboard shortcuts
        window.addEventListener('keydown', (e) => {
            if (e.repeat) return;
            const key = e.key.toLowerCase();
            if (key === 'r') Module._wasm_reset_view();
            if (key === 'p') Module._wasm_next_palette();
            if (key === 't') toggleTour();
            if (key === 'j') Module._wasm_toggle_julia();
            if (key === 'g') toggleGpu();
            if (key === 's') downloadScreenshot();
            if (e.key === 'ArrowUp') Module._wasm_adjust_iterations(10);
            if (e.key === 'ArrowDown') Module._wasm_adjust_iterations(-10);
            if (key === 'z' && (e.ctrlKey || e.metaKey)) {
                e.preventDefault();
                Module._wasm_undo_zoom();
            }
        });

        // touch support for mobile: 1-finger zoom box, 2-finger pinch
        let lastTouches = [];
        let isPinching = false;

        canvas.addEventListener('touchstart', (e) => {
            e.preventDefault();
            lastTouches = Array.from(e.touches);
            if (e.touches.length === 1) {
                isPinching = false;
                Module._wasm_mouse_down(e.touches[0].clientX, e.touches[0].clientY, 0);
            } else if (e.touches.length === 2) {
                isPinching = true;
                Module._wasm_cancel_zoom();
            }
        }, { passive: false });

        canvas.addEventListener('touchmove', (e) => {
            e.preventDefault();
            const touches = Array.from(e.touches);
            if (touches.length === 1 && !isPinching) {
                // single finger: zoom selection box
                Module._wasm_mouse_move(touches[0].clientX, touches[0].clientY);
            } else if (touches.length === 2 && lastTouches.length === 2) {
                // two fingers: pinch zoom
                isPinching = true;
                const d0 = Math.hypot(
                    lastTouches[0].clientX - lastTouches[1].clientX,
                    lastTouches[0].clientY - lastTouches[1].clientY);
                const d1 = Math.hypot(
                    touches[0].clientX - touches[1].clientX,
                    touches[0].clientY - touches[1].clientY);
                if (d0 > 0) {
                    const factor = d0 / d1;
                    const cx = (touches[0].clientX + touches[1].clientX) / 2;
                    const cy = (touches[0].clientY + touches[1].clientY) / 2;
                    Module._wasm_zoom_at(cx, cy, factor);
                }
            }
            lastTouches = touches;
        }, { passive: false });

        canvas.addEventListener('touchend', (e) => {
            if (lastTouches.length === 1 && !isPinching) {
                Module._wasm_mouse_up(lastTouches[0].clientX, lastTouches[0].clientY, 0);
            }
            lastTouches = Array.from(e.touches);
            if (e.touches.length === 0) isPinching = false;
        }, { passive: false });
    }],
    print: (text) => console.log(text),
    canvas: canvas,
    setStatus: (text) => {
        if (!text) {
            loadingScreen.style.opacity = '0';
            setTimeout(() => loadingScreen.style.display = 'none', 600);
        } else {
            statusText.textContent = text;
        }
    },
    totalDependencies: 0,
    monitorRunDependencies: (left) => {
        this.totalDependencies = Math.max(this.totalDependencies, left);
        Module.setStatus(left ? `loading dependencies (${this.totalDependencies - left}/${this.totalDependencies})...` : '');
    }
};

window.onerror = (event) => {
    statusText.textContent = "error: could not load engine.";
    statusText.style.color = "#ff5555";
};

// check for webgl2 support
const tempCanvas = document.createElement('canvas');
const gl = tempCanvas.getContext('webgl2');
if (!gl) {
    statusText.textContent = "error: webgl 2.0 not supported.";
    statusText.style.color = "#ff5555";
}
