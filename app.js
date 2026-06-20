'use strict';

// dom element references
const loadingScreen = document.getElementById('loading-screen');
const statusText = document.getElementById('status');
const canvas = document.getElementById('canvas');
const debugInfo = document.getElementById('debug-info');
const zoomBox = document.getElementById('zoom-box');

// appends a message to the splash screen log
function addLog(msg) {
    const log = document.getElementById('loading-log');
    if (log) {
        log.textContent += msg + '\n';
        log.scrollTop = log.scrollHeight;
    }
}

addLog("[loader] initializing webgl sandbox...");
addLog("[loader] loading assets/fonts/font.ttf...");
addLog("[loader] loading WebAssembly runtime...");

// configuration and runtime state
const PALETTES = ["sine wave", "grayscale", "fire", "electric", "ocean", "inferno", "viridis", "plasma", "twilight"];
let _gpuMode = true;
let _tourActive = false;
let _juliaMode = false;
let _highPrecision = false;
let _settingsOpen = false;

// transient state for jit url generation and settings synchronization
let _currentState = {
    julia_mode: false, burning_ship_mode: false, iters: 350, zoom: 3.0,
    center_re: -0.5, center_im: 0.0, palette_idx: 0,
    julia_re: 0.0, julia_im: 0.0
};

// toggles automated fractal navigation
function toggleTour() {
    Module._wasm_toggle_tour();
    _tourActive = !_tourActive;
    document.getElementById('tourBtn').textContent = _tourActive ? 'stop tour' : 'tour';
}

// switches between webgl2 fragment shader and multi-threaded wasm workers
function toggleGpu() {
    if (_gpuMode && _highPrecision) {
        // safety: force exit of high-precision mode if disabling gpu
        if (Module._wasm_toggle_precision) {
            Module._wasm_toggle_precision();
            _highPrecision = false;
        }
    }
    Module._wasm_toggle_gpu();
    _gpuMode = !_gpuMode;
    const gpuText = _gpuMode ? 'gpu ✓' : 'cpu';
    document.getElementById('gpuBtn').textContent = gpuText;
    const setGpu = document.getElementById('setGpuBtn');
    if (setGpu) setGpu.textContent = _gpuMode ? 'gpu' : 'cpu';

    updatePrecisionUI();
}

// updates the precision toggle text based on current engine mode
function updatePrecisionUI() {
    const precText = _gpuMode
        ? (_highPrecision ? '64-bit ✓' : '32-bit')
        : (_highPrecision ? '128-bit ✓' : '64-bit');

    const setPrecText = _gpuMode
        ? (_highPrecision ? '64-bit' : '32-bit')
        : (_highPrecision ? '128-bit' : '64-bit');

    const btn = document.getElementById('precisionBtn');
    if (btn) btn.textContent = precText;
    const setPrec = document.getElementById('setPrecisionBtn');
    if (setPrec) setPrec.textContent = setPrecText;
}

function closeAlert() {
    document.getElementById('alert-panel').classList.remove('active');
}

// toggles emulated 64-bit math (gpu) or native double-double (cpu)
function togglePrecision() {
    if (!_gpuMode && !_highPrecision) {
        document.getElementById('alert-panel').classList.add('active');
        return;
    }
    if (Module._wasm_toggle_precision) {
        Module._wasm_toggle_precision();
        _highPrecision = !_highPrecision;
        updatePrecisionUI();
    }
}

// toggles the slide-out advanced settings panel
function toggleSettings() {
    const panel = document.getElementById('settings-panel');
    _settingsOpen = !_settingsOpen;
    panel.classList.toggle('active', _settingsOpen);
    if (_settingsOpen) {
        // sync panel widgets with authoritative wasm state
        const setGpu = document.getElementById('setGpuBtn');
        if (setGpu) setGpu.textContent = _gpuMode ? 'gpu' : 'cpu';

        const setPrecRow = document.getElementById('setPrecisionRow');
        if (setPrecRow) setPrecRow.style.display = 'flex';

        const setPrec = document.getElementById('setPrecisionBtn');
        if (setPrec) setPrec.textContent = _gpuMode ? (_highPrecision ? '64-bit' : '32-bit') : (_highPrecision ? '128-bit' : '64-bit');

        const setPalette = document.getElementById('setPaletteSelect');
        if (setPalette) setPalette.value = _currentState.palette_idx;

        const setIters = document.getElementById('setIterationsVal');
        if (setIters) setIters.textContent = _currentState.iters;

        document.getElementById('setCenterRe').value = _currentState.center_re.toFixed(14);
        document.getElementById('setCenterIm').value = _currentState.center_im.toFixed(14);
        document.getElementById('setZoom').value = _currentState.zoom.toExponential(6);

        const juliaRow = document.getElementById('setJuliaCoordsRow');
        if (_currentState.julia_mode) {
            juliaRow.style.display = 'flex';
            document.getElementById('setJuliaRe').value = _currentState.julia_re.toFixed(14);
            document.getElementById('setJuliaIm').value = _currentState.julia_im.toFixed(14);
            if (Module._wasm_is_julia_locked) {
                const locked = Module._wasm_is_julia_locked();
                document.getElementById('setJuliaLockBtn').textContent = locked ? 'locked 🔒' : 'unlocked';
            }
        } else {
            juliaRow.style.display = 'none';
        }

        const setMode = document.getElementById('setFractalModeSelect');
        if (setMode) {
            if (_currentState.julia_mode) setMode.value = 'julia';
            else if (_currentState.burning_ship_mode) setMode.value = 'ship';
            else setMode.value = 'mandelbrot';
        }
        const setTour = document.getElementById('setTourBtn');
        if (setTour) {
            setTour.textContent = _tourActive ? 'stop tour' : 'start tour';
        }
    }
}

// pushes manual coordinate overrides from settings to the wasm engine
function updateCoordinatesFromSettings() {
    const re = parseFloat(document.getElementById('setCenterRe').value);
    const im = parseFloat(document.getElementById('setCenterIm').value);
    const z = parseFloat(document.getElementById('setZoom').value);
    if (!isNaN(re) && !isNaN(im) && !isNaN(z) && Module._wasm_set_view) {
        Module._wasm_set_view(re, im, z);
    }
    if (_currentState.julia_mode) {
        const jre = parseFloat(document.getElementById('setJuliaRe').value);
        const jim = parseFloat(document.getElementById('setJuliaIm').value);
        if (!isNaN(jre) && !isNaN(jim) && Module._wasm_set_state) {
            Module._wasm_set_state(1, jre, jim, _currentState.iters, _currentState.palette_idx);
        }
    }
}

// adjusts julia c-parameter via increments (keyboard or buttons)
function adjustJuliaC(dre, dim) {
    if (!_currentState.julia_mode) return;
    const jreInput = document.getElementById('setJuliaRe');
    const jimInput = document.getElementById('setJuliaIm');
    let jre = parseFloat(jreInput.value);
    let jim = parseFloat(jimInput.value);
    if (isNaN(jre)) jre = _currentState.julia_re;
    if (isNaN(jim)) jim = _currentState.julia_im;
    jre += dre;
    jim += dim;
    jreInput.value = jre.toFixed(14);
    jimInput.value = jim.toFixed(14);
    if (Module._wasm_set_state) {
        Module._wasm_set_state(1, jre, jim, _currentState.iters, _currentState.palette_idx);
    }
}

function toggleJuliaLock() {
    if (Module._wasm_toggle_julia_lock) {
        Module._wasm_toggle_julia_lock();
        const locked = Module._wasm_is_julia_locked();
        document.getElementById('setJuliaLockBtn').textContent = locked ? 'locked 🔒' : 'unlocked';
    }
}

function changeFractalMode(val) {
    const targetJulia = (val === 'julia');
    const targetShip = (val === 'ship');

    if (_currentState.burning_ship_mode && !targetShip && Module._wasm_toggle_burning_ship) {
        Module._wasm_toggle_burning_ship();
    }
    if (_currentState.julia_mode && !targetJulia && Module._wasm_toggle_julia) {
        Module._wasm_toggle_julia();
    }
    if (!_currentState.burning_ship_mode && targetShip && Module._wasm_toggle_burning_ship) {
        Module._wasm_toggle_burning_ship();
    }
    if (!_currentState.julia_mode && targetJulia && Module._wasm_toggle_julia) {
        Module._wasm_toggle_julia();
    }
}

function changePalette(val) {
    const idx = parseInt(val);
    if (Module._wasm_set_palette) {
        Module._wasm_set_palette(idx);
    }
}

// core telemetry callback — called by C engine every frame via EM_JS
window.updateDebugInfo = function (gpu_mode, julia_mode, burning_ship_mode, max_iters, zoom, center_re, center_im, palette_idx, tour_phase, julia_re, julia_im, high_precision, tour_target_idx, tour_total_targets, tour_target_re, tour_target_im) {
    let engine = "mandelbrot";
    if (julia_mode) engine = "julia";
    else if (burning_ship_mode) engine = "burning ship";

    if (gpu_mode) engine += high_precision ? " (gpu 64-bit)" : " (gpu 32-bit)";
    else engine += high_precision ? " (cpu 128-bit)" : " (cpu 64-bit)";

    let tour_str = "";
    if (tour_phase !== 0) {
        const phases = ["", "Panning", "Zooming in", "Zooming out"];
        const p_name = phases[tour_phase] || "";
        tour_str = `\n[tour] ${p_name} - Point ${tour_target_idx + 1}/${tour_total_targets} | Target: (${tour_target_re.toFixed(4)}, ${tour_target_im.toFixed(4)})`;
    }

    let html = `${engine}\n`;
    if (julia_mode) html += `c: (${julia_re.toFixed(10)}, ${julia_im.toFixed(10)})\n`;
    else html += `center: (${center_re.toFixed(10)}, ${center_im.toFixed(10)})\n`;

    html += `zoom: ${zoom.toPrecision(4)} | iter: ${max_iters} | palette: ${PALETTES[palette_idx % 9]}${tour_str}`;
    debugInfo.textContent = html;

    // cache state for background tasks (like url generation)
    _currentState = { julia_mode, burning_ship_mode: !!burning_ship_mode, iters: max_iters, zoom, center_re, center_im, palette_idx, julia_re, julia_im };

    // sync frontend ui with authoritative engine state
    const gpuNow = !!gpu_mode;
    const precNow = !!high_precision;
    if (_gpuMode !== gpuNow || _highPrecision !== precNow) {
        _gpuMode = gpuNow;
        _highPrecision = precNow;
        document.getElementById('gpuBtn').textContent = _gpuMode ? 'gpu \u2713' : 'cpu';
        updatePrecisionUI();
    }
    
    _tourActive = (tour_phase !== 0);
    document.getElementById('tourBtn').textContent = _tourActive ? 'stop tour' : 'tour';

    // update settings panel widgets if visible
    const setPalette = document.getElementById('setPaletteSelect');
    if (setPalette) setPalette.value = palette_idx;

    const setIters = document.getElementById('setIterationsVal');
    if (setIters) setIters.textContent = max_iters;

    if (_settingsOpen) {
        const reInput = document.getElementById('setCenterRe');
        if (reInput && document.activeElement !== reInput) reInput.value = center_re.toFixed(14);
        const imInput = document.getElementById('setCenterIm');
        if (imInput && document.activeElement !== imInput) imInput.value = center_im.toFixed(14);
        const zInput = document.getElementById('setZoom');
        if (zInput && document.activeElement !== zInput) zInput.value = zoom.toExponential(6);
        const jreInput = document.getElementById('setJuliaRe');
        if (jreInput && document.activeElement !== jreInput) jreInput.value = julia_re.toFixed(14);
        const jimInput = document.getElementById('setJuliaIm');
        if (jimInput && document.activeElement !== jimInput) jimInput.value = julia_im.toFixed(14);
        
        const lockBtn = document.getElementById('setJuliaLockBtn');
        if (lockBtn && Module._wasm_is_julia_locked) {
            const locked = Module._wasm_is_julia_locked();
            lockBtn.textContent = locked ? 'locked 🔒' : 'unlocked';
        }
        
        const juliaRow = document.getElementById('setJuliaCoordsRow');
        if (juliaRow) juliaRow.style.display = julia_mode ? 'flex' : 'none';
        
        const setMode = document.getElementById('setFractalModeSelect');
        if (setMode) {
            if (julia_mode) setMode.value = 'julia';
            else if (burning_ship_mode) setMode.value = 'ship';
            else setMode.value = 'mandelbrot';
        }
    }
};

// serializes current view state into browser url parameters
function updateURL() {
    const s = _currentState;
    const params = new URLSearchParams(window.location.search);
    params.set('re', s.center_re.toFixed(14));
    params.set('im', s.center_im.toFixed(14));
    params.set('z', s.zoom.toExponential(6));
    if (s.julia_mode) {
        params.set('j', '1');
        params.set('jre', s.julia_re.toFixed(14));
        params.set('jim', s.julia_im.toFixed(14));
    } else {
        params.delete('j'); params.delete('jre'); params.delete('jim');
    }
    params.set('it', s.iters);
    params.set('p', s.palette_idx);

    const newUrl = window.location.pathname + '?' + params.toString();
    window.history.replaceState(null, '', newUrl);
    return window.location.href;
}

// restores engine state from browser url parameters
function loadFromURL() {
    const params = new URLSearchParams(window.location.search);
    const re = parseFloat(params.get('re')), im = parseFloat(params.get('im')), z = parseFloat(params.get('z'));
    const j = params.get('j') === '1';
    const jre = parseFloat(params.get('jre')) || 0, jim = parseFloat(params.get('jim')) || 0;
    const it = parseInt(params.get('it')) || 0, p = parseInt(params.get('p')) || 0;

    if (!isNaN(re) && !isNaN(im) && !isNaN(z)) {
        if (Module._wasm_set_view) Module._wasm_set_view(re, im, z);
        if (Module._wasm_set_state) Module._wasm_set_state(j ? 1 : 0, jre, jim, it, p);
    }
}

// generates a deep link and copies it to clipboard
function copyLink() {
    const btn = document.getElementById('copyBtn');
    const url = updateURL(); 
    navigator.clipboard.writeText(url).then(() => {
        const oldText = btn.textContent;
        btn.textContent = 'copied!';
        setTimeout(() => btn.textContent = oldText, 2000);
    }).catch(err => { alert("Link: " + url); });
}

// visual helper callback for zooming interaction
window.updateZoomBox = function (is_zooming, x, y, w, h) {
    if (is_zooming) {
        zoomBox.style.display = 'block';
        zoomBox.style.left = x + 'px'; zoomBox.style.top = y + 'px';
        zoomBox.style.width = w + 'px'; zoomBox.style.height = h + 'px';
    } else {
        zoomBox.style.display = 'none';
    }
};

// triggers frame download — called from C engine after glReadPixels
window.downloadScreenshotData = function (ptr, w, h, heap) {
    if (!ptr || w <= 0 || h <= 0) return;
    const wasmHeap = heap || Module.HEAPU8;
    
    // transform wasm memory range to js image data
    const data = new Uint8ClampedArray(wasmHeap.buffer, ptr, w * h * 4);
    const pixels = new Uint8ClampedArray(data);
    const imgData = new ImageData(pixels, w, h);

    const tmpCanvas = document.createElement('canvas');
    tmpCanvas.width = w; tmpCanvas.height = h;
    const ctx2d = tmpCanvas.getContext('2d');
    ctx2d.putImageData(imgData, 0, 0);

    const link = document.createElement('a');
    const now = new Date();
    const ts = now.getFullYear().toString() +
        String(now.getMonth() + 1).padStart(2, '0') +
        String(now.getDate()).padStart(2, '0') + '_' +
        String(now.getHours()).padStart(2, '0') +
        String(now.getMinutes()).padStart(2, '0') +
        String(now.getSeconds()).padStart(2, '0');
    link.download = `mandelbrot_${ts}.png`;
    link.href = tmpCanvas.toDataURL('image/png');
    link.click();
};

function syncSize() {
    const w = window.innerWidth, h = window.innerHeight;
    if (Module._wasm_set_resolution) Module._wasm_set_resolution(w, h);
}

function downloadScreenshot() {
    if (Module._wasm_request_screenshot) Module._wasm_request_screenshot();
}

// main emscripten module configuration
var Module = {
    preRun: [],
    postRun: [function () {
        loadFromURL();
        syncSize();
        window.addEventListener('resize', () => {
            clearTimeout(window._rt);
            window._rt = setTimeout(syncSize, 100);
        });

        // raw input mapping (authoritative state lives in C)
        window.addEventListener('keydown', (e) => {
            if (e.repeat) return;
            const key = e.key.toLowerCase();
            if (key === 'r') Module._wasm_reset_view();
            if (key === 'p') Module._wasm_next_palette();
            if (key === 't') toggleTour();
            if (key === 'j') Module._wasm_toggle_julia();
            if (key === 'g') toggleGpu();
            if (key === 'e') togglePrecision();
            if (key === 's') downloadScreenshot();
            if (e.key === 'ArrowUp') Module._wasm_adjust_iterations(10);
            if (e.key === 'ArrowDown') Module._wasm_adjust_iterations(-10);
            if (key === 'z' && (e.ctrlKey || e.metaKey)) {
                e.preventDefault();
                Module._wasm_undo_zoom();
            }
        });

        // touch interaction: maps gestures to wasm mouse emulation
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
                Module._wasm_mouse_move(touches[0].clientX, touches[0].clientY);
            } else if (touches.length === 2 && lastTouches.length === 2) {
                isPinching = true;
                const d0 = Math.hypot(lastTouches[0].clientX - lastTouches[1].clientX, lastTouches[0].clientY - lastTouches[1].clientY);
                const d1 = Math.hypot(touches[0].clientX - touches[1].clientX, touches[0].clientY - touches[1].clientY);
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
    print: (text) => { console.log(text); addLog(text); },
    printErr: (text) => { console.error(text); addLog("[stderr] " + text); },
    canvas: canvas,
    setStatus: (text) => {
        if (!text) {
            addLog("[loader] WebAssembly runtime initialized.");
            loadingScreen.style.opacity = '0';
            setTimeout(() => loadingScreen.style.display = 'none', 600);
        } else {
            statusText.textContent = text;
            addLog(`[loader] ${text}`);
        }
    }
};

window.onerror = () => {
    statusText.textContent = "error: could not load engine.";
    statusText.style.color = "#ff5555";
};

// basic webgl 2.0 environment probe
const tempCanvas = document.createElement('canvas');
const gl = tempCanvas.getContext('webgl2');
if (!gl) {
    statusText.textContent = "error: webgl 2.0 not supported.";
    statusText.style.color = "#ff5555";
}
tempCanvas.remove();
