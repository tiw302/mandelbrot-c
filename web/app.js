'use strict';

// dom element references
const loadingScreen = document.getElementById('loading-screen');
const statusText = document.getElementById('status');
const canvas = document.getElementById('canvas');
const debugInfo = document.getElementById('debug-info');
const zoomBox = document.getElementById('zoom-box');

let _systemLogs = "";

// appends a message to the splash screen log and global log storage
function addLog(msg) {
    _systemLogs += msg + '\n';
    const log = document.getElementById('loading-log');
    if (log) {
        log.textContent = _systemLogs;
        log.scrollTop = log.scrollHeight;
    }
}

function showSystemLogs() {
    if (typeof _settingsOpen !== 'undefined' && _settingsOpen) {
        toggleSettings();
    }
    const screen = document.getElementById('loading-screen');
    const spinner = screen.querySelector('.spinner');
    if (spinner) spinner.style.display = 'none';
    const statusText = document.getElementById('status');
    if (statusText) statusText.textContent = "--- system logs ---";

    let closeBtn = document.getElementById('log-close-btn');
    if (!closeBtn) {
        closeBtn = document.createElement('button');
        closeBtn.id = 'log-close-btn';
        closeBtn.textContent = 'close logs';
        closeBtn.className = 'settings-close';
        closeBtn.style.marginTop = '15px';
        closeBtn.onclick = hideSystemLogs;
        screen.appendChild(closeBtn);
    }
    closeBtn.style.display = 'block';

    screen.style.display = 'flex';
    // Small delay to allow display:flex to apply before transitioning opacity
    setTimeout(() => { screen.style.opacity = '1'; }, 10);
}

function hideSystemLogs() {
    const screen = document.getElementById('loading-screen');
    screen.style.opacity = '0';
    setTimeout(() => {
        screen.style.display = 'none';
    }, 600);
}

addLog("[loader] initializing webgl sandbox...");
addLog("[loader] loading assets/fonts/font.ttf...");
addLog("[loader] loading WebAssembly runtime...");

// configuration and runtime state
const PALETTES = ["sine wave", "volumetric magma", "viridis", "grayscale", "electric", "ocean", "inferno", "retro binary", "orbit mesh (gpu)", "biomorph trap (gpu)", "conformal ripples (gpu)", "curvature marble (gpu)", "conformal grid (gpu)", "cyber grid (gpu)", "bubble pearl (gpu)", "liquid chrome (gpu)", "refractive 3d glass (gpu)", "ultra fractal classic (gpu)", "pure binary bw", "classic royal blue (gpu)", "classic fire red (gpu)", "silver crimson (gpu)"];
let _gpuMode = true;
let _tourActive = false;
let _juliaMode = false;
let _highPrecision = false;
let _settingsOpen = false;

// Prevent UI clicks from leaking through to the canvas (which cancels tours/interactions)
document.addEventListener('DOMContentLoaded', () => {
    const uiElements = ['toolbar', 'settings-panel', 'alert-panel', 'info-panel'];
    uiElements.forEach(id => {
        const el = document.getElementById(id);
        if (el) {
            const stop = e => e.stopPropagation();
            el.addEventListener('mousedown', stop);
            el.addEventListener('mouseup', stop);
            el.addEventListener('touchstart', stop, {passive: true});
            el.addEventListener('touchend', stop, {passive: true});
            el.addEventListener('wheel', stop, {passive: true});
            el.addEventListener('pointerdown', stop);
            el.addEventListener('pointerup', stop);
            el.addEventListener('click', stop);
            el.addEventListener('dblclick', stop);
        }
    });
});

// transient state for jit url generation and settings synchronization
let _currentState = {
    julia_mode: false, base_fractal: 0, iters: 350, zoom: 3.0,
    center_re: -0.5, center_im: 0.0, palette_idx: 0,
    julia_re: 0.0, julia_im: 0.0
};

// toggles automated fractal navigation
function toggleTour() {
    Module._wasm_toggle_tour();
    _tourActive = !_tourActive;
    document.getElementById('tourBtn').innerHTML = _tourActive ? '<img src="assets/icons/stop.svg" class="btn-icon">stop tour (T)' : '<img src="assets/icons/tour.svg" class="btn-icon">tour (T)';
    const setTour = document.getElementById('setTourBtn');
    if (setTour) {
        setTour.innerHTML = _tourActive ? '<img src="assets/icons/stop.svg" class="btn-icon">stop tour (T)' : '<img src="assets/icons/tour.svg" class="btn-icon">start tour (T)';
    }
}

function updateTourSpeed(speed) {
    if (Module._wasm_set_tour_speed) {
        Module._wasm_set_tour_speed(parseFloat(speed));
    }
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
    const gpuText = _gpuMode ? 'gpu ✓ (G)' : 'cpu (G)';
    document.getElementById('gpuBtn').innerHTML = '<img src="assets/icons/gpu.svg" class="btn-icon">' + gpuText;
    const setGpu = document.getElementById('setGpuBtn');
    if (setGpu) setGpu.textContent = _gpuMode ? 'gpu' : 'cpu';

    updatePrecisionUI();
}

// updates the precision toggle text based on current engine mode
function updatePrecisionUI() {
    const precText = _gpuMode
        ? (_highPrecision ? '64-bit ✓ (E)' : '32-bit (E)')
        : (_highPrecision ? '128-bit ✓ (E)' : '64-bit (E)');

    const setPrecText = _gpuMode
        ? (_highPrecision ? '64-bit' : '32-bit')
        : (_highPrecision ? '128-bit' : '64-bit');

    const btn = document.getElementById('precisionBtn');
    if (btn) btn.innerHTML = '<img src="assets/icons/precision.svg" class="btn-icon">' + precText;
    const setPrec = document.getElementById('setPrecisionBtn');
    if (setPrec) setPrec.textContent = setPrecText;
}

function closeAlert() {
    document.getElementById('alert-panel').classList.remove('active');
}

function toggleInfo() {
    const panel = document.getElementById('info-panel');
    panel.classList.toggle('active');
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
                const lockSvg = locked ? '<img src="assets/icons/lock.svg" class="btn-icon">' : '<img src="assets/icons/unlock.svg" class="btn-icon">';
                document.getElementById('setJuliaLockBtn').innerHTML = lockSvg + (locked ? 'locked' : 'unlocked');
            }
        } else {
            juliaRow.style.display = 'none';
        }

        const setMode = document.getElementById('setFractalModeSelect');
        if (setMode) {
            if (_currentState.julia_mode) {
                setMode.value = 'julia';
            } else {
                let name = 'mandelbrot';
                if (Module._wasm_get_registered_count) {
                    const count = Module._wasm_get_registered_count();
                    for (let i = 0; i < count; i++) {
                        if (Module._wasm_get_registered_mode(i) === _currentState.base_fractal) {
                            name = UTF8ToString(Module._wasm_get_registered_name(i));
                            break;
                        }
                    }
                }
                setMode.value = name;
            }
        }
        const setTour = document.getElementById('setTourBtn');
        if (setTour) {
            setTour.innerHTML = _tourActive ? '<img src="assets/icons/stop.svg" class="btn-icon">stop tour (T)' : '<img src="assets/icons/tour.svg" class="btn-icon">start tour (T)';
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
        const lockSvg = locked ? '<img src="assets/icons/lock.svg" class="btn-icon">' : '<img src="assets/icons/unlock.svg" class="btn-icon">';

        const lockBtn = document.getElementById('setJuliaLockBtn');
        if (lockBtn) lockBtn.innerHTML = lockSvg + (locked ? 'locked' : 'unlocked');

        const toolbarLockBtn = document.getElementById('juliaLockToolbarBtn');
        if (toolbarLockBtn) toolbarLockBtn.innerHTML = lockSvg + (locked ? 'locked (L)' : 'unlocked (L)');

        window._lastLockedState = locked;
    }
}

function changeFractalMode(val) {
    if (val === 'julia') {
        if (!_currentState.julia_mode && Module._wasm_toggle_julia) {
            Module._wasm_toggle_julia();
        }
    } else {
        if (_currentState.julia_mode && Module._wasm_toggle_julia) {
            Module._wasm_toggle_julia();
        }

        let mode = 0;
        if (Module._wasm_get_registered_count) {
            const count = Module._wasm_get_registered_count();
            for (let i = 0; i < count; i++) {
                const name = UTF8ToString(Module._wasm_get_registered_name(i));
                if (name === val) {
                    mode = Module._wasm_get_registered_mode(i);
                    break;
                }
            }
        }

        if (Module._wasm_set_fractal_mode) {
            Module._wasm_set_fractal_mode(mode);
        }
    }
}

function changePalette(val) {
    const idx = parseInt(val);
    if (Module._wasm_set_palette) {
        Module._wasm_set_palette(idx);
    }
}

// core telemetry callback — called by C engine every frame via EM_JS
window.updateDebugInfo = function (gpu_mode, julia_mode, base_fractal, max_iters, zoom, center_re, center_im, palette_idx, tour_phase, julia_re, julia_im, high_precision, tour_target_idx, tour_total_targets, tour_target_re, tour_target_im, thread_count, render_time_ms) {
    let engine = "CPU";
    let precision = "Double (64-bit)";
    if (gpu_mode) {
        engine = "GPU";
        precision = high_precision ? "Double (64-bit emulation)" : "Double (32-bit)";
    } else {
        engine = "CPU";
        precision = high_precision ? "Double-double (128-bit)" : "Double (64-bit)";
    }

    let baseFractalName = "Unknown";
    if (base_fractal === 0) baseFractalName = "Mandelbrot";
    else if (base_fractal === 1) baseFractalName = "Burning Ship";
    else if (base_fractal === 2) baseFractalName = "Tricorn";
    else if (base_fractal === 3) baseFractalName = "Celtic";
    else if (base_fractal === 4) baseFractalName = "Buffalo";

    let juliaStatus = julia_mode ? "ON" : "OFF";
    let html = `[ENGINE]    ${engine} | Fractal: ${baseFractalName} | Julia: ${juliaStatus} | Threads: ${thread_count} | Render: ${render_time_ms} ms\n`;
    html += `[PRECISION] <span style="color: #66ff66">${precision}</span>\n`;

    if (julia_mode) {
        html += `[COORD]     C: (${julia_re.toFixed(14)}, ${julia_im.toFixed(14)})\n`;
    } else {
        html += `[COORD]     Center: (${center_re.toFixed(14)}, ${center_im.toFixed(14)})\n`;
    }

    html += `[RENDER]    Zoom: ${zoom.toExponential(6)} | Iters: ${max_iters} | Palette: ${PALETTES[palette_idx % PALETTES.length]}`;

    if (tour_phase !== 0) {
        if (julia_mode) {
            const phases = ["", "Morphing", "Dwelling"];
            const p_name = phases[tour_phase] || "";
            html += `\n[TOUR]      Auto-Julia [${p_name}] Target #${tour_target_idx + 1}/${tour_total_targets} (${tour_target_re.toFixed(4)}, ${tour_target_im.toFixed(4)})`;
        } else {
            const phases = ["", "Panning", "Zooming in", "Zooming out"];
            const p_name = phases[tour_phase] || "";
            html += `\n[TOUR]      Auto-Zoom [${p_name}] Target #${tour_target_idx + 1}/${tour_total_targets} (${tour_target_re.toFixed(4)}, ${tour_target_im.toFixed(4)})`;
        }
    }

    debugInfo.innerHTML = html;

    // cache state for background tasks (like url generation)
    _currentState = { julia_mode, base_fractal: base_fractal, iters: max_iters, zoom, center_re, center_im, palette_idx, julia_re, julia_im };

    // sync frontend ui with authoritative engine state
    const gpuNow = !!gpu_mode;
    const precNow = !!high_precision;
    if (_gpuMode !== gpuNow || _highPrecision !== precNow) {
        _gpuMode = gpuNow;
        _highPrecision = precNow;
        document.getElementById('gpuBtn').innerHTML = '<img src="assets/icons/gpu.svg" class="btn-icon">' + (_gpuMode ? 'gpu ✓ (G)' : 'cpu (G)');
        updatePrecisionUI();
    }

    const tourNow = (tour_phase !== 0);
    if (_tourActive !== tourNow) {
        _tourActive = tourNow;
        document.getElementById('tourBtn').innerHTML = _tourActive ? '<img src="assets/icons/stop.svg" class="btn-icon">stop tour (T)' : '<img src="assets/icons/tour.svg" class="btn-icon">tour (T)';
        const setTour = document.getElementById('setTourBtn');
        if (setTour) {
            setTour.innerHTML = _tourActive ? '<img src="assets/icons/stop.svg" class="btn-icon">stop tour (T)' : '<img src="assets/icons/tour.svg" class="btn-icon">start tour (T)';
        }
    }
    // update settings panel widgets if visible
    const setPalette = document.getElementById('setPaletteSelect');
    if (setPalette) setPalette.value = palette_idx;

    const setIters = document.getElementById('setIterationsVal');
    if (setIters) setIters.textContent = max_iters;

    const toolbarLockBtn = document.getElementById('juliaLockToolbarBtn');
    if (Module._wasm_is_julia_locked) {
        const locked = Module._wasm_is_julia_locked();
        if (window._lastLockedState !== locked) {
            window._lastLockedState = locked;
            const lockSvg = locked ? '<img src="assets/icons/lock.svg" class="btn-icon">' : '<img src="assets/icons/unlock.svg" class="btn-icon">';
            if (toolbarLockBtn) toolbarLockBtn.innerHTML = lockSvg + (locked ? 'locked (L)' : 'unlocked (L)');
            const lockBtn = document.getElementById('setJuliaLockBtn');
            if (lockBtn) lockBtn.innerHTML = lockSvg + (locked ? 'locked' : 'unlocked');
        }
    }

    if (toolbarLockBtn) {
        toolbarLockBtn.style.display = julia_mode ? 'inline-block' : 'none';
    }

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

        const juliaRow = document.getElementById('setJuliaCoordsRow');
        if (juliaRow) juliaRow.style.display = julia_mode ? 'flex' : 'none';

        const setJuliaBtn = document.getElementById('setJuliaBtn');
        if (setJuliaBtn) setJuliaBtn.textContent = julia_mode ? 'ON' : 'OFF';

        const setMode = document.getElementById('setFractalModeSelect');
        if (setMode) {
            if (julia_mode) {
                setMode.value = 'julia';
            } else {
                let name = 'mandelbrot';
                if (Module._wasm_get_registered_count) {
                    const count = Module._wasm_get_registered_count();
                    for (let i = 0; i < count; i++) {
                        if (Module._wasm_get_registered_mode(i) === base_fractal) {
                            name = UTF8ToString(Module._wasm_get_registered_name(i));
                            break;
                        }
                    }
                }
                setMode.value = name;
            }
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
    params.set('f', s.base_fractal);

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
    const f = parseInt(params.get('f')) || 0;

    if (!isNaN(re) && !isNaN(im) && !isNaN(z)) {
        if (Module._wasm_set_view) Module._wasm_set_view(re, im, z);
        if (Module._wasm_set_state) Module._wasm_set_state(j ? 1 : 0, jre, jim, it, p);
        if (Module._wasm_set_fractal_mode) Module._wasm_set_fractal_mode(f);
    }
}

// generates a deep link and copies it to clipboard
function copyLink() {
    const btn = document.getElementById('copyBtn');
    const url = updateURL();
    navigator.clipboard.writeText(url).then(() => {
        const oldHtml = btn.innerHTML;
        btn.innerHTML = '<img src="assets/icons/copy.svg" class="btn-icon">copied!';
        setTimeout(() => btn.innerHTML = oldHtml, 2000);
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
window.downloadScreenshotData = function (ptr, w, h, heap, is_bottom_up) {
    if (!ptr || w <= 0 || h <= 0) return;
    const wasmHeap = heap || Module.HEAPU8;

    // flash animation
    const flash = document.createElement('div');
    flash.style.position = 'fixed';
    flash.style.top = '0'; flash.style.left = '0';
    flash.style.width = '100vw'; flash.style.height = '100vh';
    flash.style.backgroundColor = 'white';
    flash.style.opacity = '0.8';
    flash.style.zIndex = '9999';
    flash.style.transition = 'opacity 0.5s ease-out';
    flash.style.pointerEvents = 'none';
    document.body.appendChild(flash);
    setTimeout(() => { flash.style.opacity = '0'; }, 50);
    setTimeout(() => { document.body.removeChild(flash); }, 600);

    // transform wasm memory range to js image data
    const data = new Uint8ClampedArray(wasmHeap.buffer, ptr, w * h * 4);
    const pixels = new Uint8ClampedArray(data);
    const imgData = new ImageData(pixels, w, h);

    const tmpCanvas = document.createElement('canvas');
    tmpCanvas.width = w; tmpCanvas.height = h;
    const ctx2d = tmpCanvas.getContext('2d');

    if (is_bottom_up) {
        // webgl pixels are bottom-up, need to flip vertically
        const idCanvas = document.createElement('canvas');
        idCanvas.width = w; idCanvas.height = h;
        idCanvas.getContext('2d').putImageData(imgData, 0, 0);
        ctx2d.translate(0, h);
        ctx2d.scale(1, -1);
        ctx2d.drawImage(idCanvas, 0, 0);
    } else {
        ctx2d.putImageData(imgData, 0, 0);
    }

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
            const tagName = document.activeElement ? document.activeElement.tagName : '';
            if (tagName === 'INPUT' || tagName === 'SELECT') return;
            const key = e.key.toLowerCase();
            if (key === 'r') Module._wasm_reset_view();
            if (key === 'p') Module._wasm_next_palette();
            if (key === 't') toggleTour();
            if (key === 'j') Module._wasm_toggle_julia();
            if (key === 'g') toggleGpu();
            if (key === 'e') togglePrecision();
            if (key === 's') downloadScreenshot();
            if (key === 'l') toggleJuliaLock();
            if (key === 'c') copyLink();
            if (key === 'i') toggleInfo();
            if (key === 'o') toggleSettings();
            if (key === 'f' || key === 'b') {
                if (Module._wasm_cycle_fractal) Module._wasm_cycle_fractal();
            }
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

            // dynamically populate dropdown from WASM registry
            if (Module._wasm_get_registered_count) {
                const count = Module._wasm_get_registered_count();
                const setMode = document.getElementById('setFractalModeSelect');
                if (setMode) {
                    setMode.innerHTML = '';
                    for (let i = 0; i < count; i++) {
                        const name = UTF8ToString(Module._wasm_get_registered_name(i));
                        if (name === "julia") continue; // skip julia because it has a dedicated toggle
                        const displayName = UTF8ToString(Module._wasm_get_registered_display_name(i));
                        const opt = document.createElement('option');
                        opt.value = name;
                        opt.textContent = displayName;
                        setMode.appendChild(opt);
                    }
                }
            }

            if (Module._wasm_get_palette_count) {
                const count = Module._wasm_get_palette_count();
                const setPalette = document.getElementById('setPaletteSelect');
                if (setPalette) {
                    setPalette.innerHTML = '';
                    for (let i = 0; i < count; i++) {
                        const name = UTF8ToString(Module._wasm_get_palette_name(i));
                        const opt = document.createElement('option');
                        opt.value = i;
                        opt.textContent = name;
                        setPalette.appendChild(opt);
                    }
                }
            }

            loadingScreen.style.opacity = '0';
            setTimeout(() => loadingScreen.style.display = 'none', 600);
        } else {
            statusText.textContent = text;
            addLog(`[loader] ${text}`);
        }
    }
};

function showFatalError(msg) {
    statusText.textContent = "error: " + msg;
    statusText.style.color = "#ff5555";
    const overlay = document.getElementById('error-overlay');
    const errorMsg = document.getElementById('error-message');
    if (overlay && errorMsg) {
        overlay.style.display = 'flex';
        errorMsg.textContent = msg;
    }
}

window.onerror = (message, source, lineno, colno, error) => {
    showFatalError(message || "could not load engine.");
};

// basic webgl 2.0 environment probe
const tempCanvas = document.createElement('canvas');
const gl = tempCanvas.getContext('webgl2');
if (!gl) {
    showFatalError("webgl 2.0 not supported.");
}
tempCanvas.remove();
