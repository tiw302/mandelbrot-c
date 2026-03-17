#ifndef CONFIG_H
#define CONFIG_H

/* Window dimensions */
#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

/* Iteration limit. Higher = more detail but slower.
   Recommended: 500 (fast), 1000 (balanced), 2000+ (deep zooms) */
#define MAX_ITERATIONS  1000

/* Escape radius. Standard is 2.0 -- no need to change this. */
#define ESCAPE_RADIUS   2.0

/* Number of render threads. Set this to your CPU core count.
   Check with: nproc (Linux/Mac) or Task Manager > Performance (Windows) */
#define THREAD_COUNT    8

/* Show the debug overlay (render time, coordinates, zoom level).
   1 = visible, 0 = hidden */
#define DEBUG_INFO      1
#define FONT_SIZE       16

/* Font paths -- the program tries each one in order until it finds a valid font.
   If none are found, the debug overlay is silently disabled. */
#if defined(_WIN32) || defined(_WIN64)
    #define FONT_PATH_1  "C:\\Windows\\Fonts\\arial.ttf"
    #define FONT_PATH_2  "C:\\Windows\\Fonts\\segoeui.ttf"
    #define FONT_PATH_3  "C:\\Windows\\Fonts\\calibri.ttf"
    #define FONT_PATH_4  ""
#elif defined(__APPLE__) || defined(__MACH__)
    #define FONT_PATH_1  "/System/Library/Fonts/Helvetica.ttc"
    #define FONT_PATH_2  "/System/Library/Fonts/SFNS.ttf"
    #define FONT_PATH_3  "/Library/Fonts/Arial.ttf"
    #define FONT_PATH_4  "/opt/homebrew/share/fonts/dejavu-fonts/DejaVuSans.ttf"
#else
    #define FONT_PATH_1  "/usr/share/fonts/TTF/DejaVuSans.ttf"
    #define FONT_PATH_2  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    #define FONT_PATH_3  "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
    #define FONT_PATH_4  "/usr/share/fonts/noto/NotoSans-Regular.ttf"
#endif

/* How many zoom steps to remember for undo (Ctrl+Z) */
#define MAX_HISTORY_SIZE 100

/* Starting view: centered slightly left of the origin to show the full set */
#define INITIAL_CENTER_RE   -0.5
#define INITIAL_CENTER_IM    0.0
#define INITIAL_ZOOM         3.0

#endif /* CONFIG_H */
