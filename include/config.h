/* config.h
 *
 * default application settings and constants.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "math_types.h"

// initial window dimensions
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

#if defined(__EMSCRIPTEN__)
// web target defaults: lower depth for better browser responsiveness
#define DEFAULT_ITERATIONS 350
#else
// desktop target defaults: higher depth for native performance
#define DEFAULT_ITERATIONS 1200
#endif

// absolute upper bound for iteration depth
#define MAX_ITERATIONS_LIMIT 10000

// squared distance threshold for divergence
#define ESCAPE_RADIUS 10.0

// parallel execution control (0 = auto-detect)
#define DEFAULT_THREAD_COUNT 0

// visual and ui configuration
#define DEBUG_INFO 1
#define FONT_SIZE 15
#define DEFAULT_PALETTE 0

// filesystem paths for font assets
#define FONT_PATH_LOCAL "assets/fonts/font.ttf"

/* fallback font paths for cross-platform compatibility.
 * these are probed sequentially if the local asset is missing. */
#if defined(_WIN32) || defined(_WIN64)
#define FONT_PATH_1 "C:\\Windows\\Fonts\\arial.ttf"
#define FONT_PATH_2 "C:\\Windows\\Fonts\\segoeui.ttf"
#define FONT_PATH_3 "C:\\Windows\\Fonts\\calibri.ttf"
#define FONT_PATH_4 ""
#elif defined(__APPLE__) || defined(__MACH__)
#define FONT_PATH_1 "/System/Library/Fonts/Helvetica.ttc"
#define FONT_PATH_2 "/System/Library/Fonts/SFNS.ttf"
#define FONT_PATH_3 "/Library/Fonts/Arial.ttf"
#define FONT_PATH_4 "/opt/homebrew/share/fonts/dejavu-fonts/DejaVuSans.ttf"
#else
#define FONT_PATH_1 "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define FONT_PATH_2 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define FONT_PATH_3 "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
#define FONT_PATH_4 "/usr/share/fonts/noto/NotoSans-Regular.ttf"
#endif

// maximum depth of the navigation history stack
#define MAX_HISTORY_SIZE 100

// default viewport coordinates and magnification
#define INITIAL_CENTER_RE -0.5
#define INITIAL_CENTER_IM 0.0
#define INITIAL_ZOOM 3.0

/* zoom threshold below which perturbation theory is used for the reference orbit.
 * dekker double-single (64-bit emulation on 32-bit gpu floats) handles ~14 decimal
 * digits of precision, covering zoom levels down to ~1e-13 without artifacts.
 * perturbation is only necessary past that point. setting the threshold at 1e-13
 * ensures dekker is used in the comfortable [1e-5, 1e-13] range and perturbation
 * kicks in only where it is actually required. */
#define PERTURBATION_ZOOM_THRESHOLD 1e-13

/* zoom threshold below which the reference orbit is computed using BigNum
 * instead of double-precision. kept for documentation; the bignum code path
 * is currently disabled because the camera is capped at 1e-32 (within __float128
 * precision range) so this threshold is never reached in practice. */
#define BIGNUM_ZOOM_THRESHOLD 1e-60

#endif // CONFIG_H
