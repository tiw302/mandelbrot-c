#ifndef CONFIG_H
#define CONFIG_H

/* Window dimensions */
#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

/* Iteration limit: higher values increase detail but decrease performance */
#define MAX_ITERATIONS  1000

/* Escape threshold: larger values improve smooth coloring accuracy */
#define ESCAPE_RADIUS   10.0

/* Parallel rendering thread count */
#define THREAD_COUNT    8

/* Debug overlay configuration */
#define DEBUG_INFO      1
#define FONT_SIZE       16

/* System-specific font fallback paths */
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

/* Maximum steps for undo history (Ctrl+Z) */
#define MAX_HISTORY_SIZE 100

/* Initial view coordinates and zoom level */
#define INITIAL_CENTER_RE   -0.5
#define INITIAL_CENTER_IM    0.0
#define INITIAL_ZOOM         3.0

#endif /* CONFIG_H */
