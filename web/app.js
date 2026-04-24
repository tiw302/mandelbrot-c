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

// wasm callbacks
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

    // throttled url update
    updateURL(julia_mode, max_iters, zoom, center_re, center_im, palette_idx, julia_re, julia_im);
};

let _lastUrlUpdate = 0;
let _urlInitialized = false;
function updateURL(julia_mode, iters, zoom, center_re, center_im, palette_idx, julia_re, julia_im) {
    if (!_urlInitialized) return;
    const now = Date.now();
    if (now - _lastUrlUpdate < 500) return;
    _lastUrlUpdate = now;

    const params = new URLSearchParams(window.location.search);
    params.set('re', center_re.toFixed(14));
    params.set('im', center_im.toFixed(14));
    params.set('z', zoom.toExponential(6));
    if (julia_mode) {
        params.set('j', '1');
        params.set('jre', julia_re.toFixed(14));
        params.set('jim', julia_im.toFixed(14));
    } else {
        params.delete('j');
        params.delete('jre');
        params.delete('jim');
    }
    params.set('it', iters);
    params.set('p', palette_idx);

    const newUrl = window.location.pathname + '?' + params.toString();
    window.history.replaceState(null, '', newUrl);
}

function loadFromURL() {
    const params = new URLSearchParams(window.location.search);
    const re = parseFloat(params.get('re'));
    const im = parseFloat(params.get('im'));
    const z = parseFloat(params.get('z'));
    const j = params.get('j') === '1';
    const jre = parseFloat(params.get('jre')) || 0;
    const jim = parseFloat(params.get('jim')) || 0;
    const it = parseInt(params.get('it')) || 0;
    const p = parseInt(params.get('p')) || 0;

    if (!isNaN(re) && !isNaN(im) && !isNaN(z)) {
        console.log(`[url] loading coordinates: re=${re}, im=${im}, z=${z}`);
        if (Module._wasm_set_view) {
            Module._wasm_set_view(re, im, z);
        }
        if (Module._wasm_set_state) {
            Module._wasm_set_state(j ? 1 : 0, jre, jim, it, p);
        }
    } else {
        console.log("[url] no coordinates found or invalid format, using defaults");
    }
    
    // delay enabling updates to let the engine settle
    setTimeout(() => {
        _urlInitialized = true;
        console.log("[url] updates enabled");
    }, 500);
}

function copyLink() {
    const btn = document.getElementById('copyBtn');
    navigator.clipboard.writeText(window.location.href).then(() => {
        const oldText = btn.textContent;
        btn.textContent = 'copied!';
        setTimeout(() => btn.textContent = oldText, 2000);
    }).catch(err => {
        alert("Link: " + window.location.href);
    });
}

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

window.downloadScreenshotData = function(ptr, w, h, heap) {
    if (!ptr || w <= 0 || h <= 0) return;
    
    const wasmHeap = heap || Module.HEAPU8;
    if (!wasmHeap) {
        console.error("WASM Memory is not available.");
        return;
    }
    
    // copy pixels from wasm heap
    const data = new Uint8ClampedArray(wasmHeap.buffer, ptr, w * h * 4);
    const pixels = new Uint8ClampedArray(data);
    const imgData = new ImageData(pixels, w, h);
    
    // generate png via temp canvas
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

// emscripten config
var Module = {
    preRun: [],
    postRun: [function() {
        loadFromURL();
        syncSize();
        window.addEventListener('resize', () => {
            clearTimeout(window._rt);
            window._rt = setTimeout(syncSize, 100);
        });

        // keyboard events
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

        // mobile touch: 1-finger zoom, 2-finger pinch
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

// webgl2 check
const tempCanvas = document.createElement('canvas');
const gl = tempCanvas.getContext('webgl2');
if (!gl) {
    statusText.textContent = "error: webgl 2.0 not supported.";
    statusText.style.color = "#ff5555";
}
