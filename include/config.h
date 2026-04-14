#ifndef CONFIG_H
#define CONFIG_H

/* display resolution */
#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

/* more iterations = more detail, but slower */
#define DEFAULT_ITERATIONS 1000
#define MAX_ITERATIONS_LIMIT 10000

/* bigger radius = smoother gradients */
#define ESCAPE_RADIUS   10

/* worker threads for parallel rendering (0 = auto-detect) */
#define DEFAULT_THREAD_COUNT 0

/* debug overlay toggle and text size */
#define DEBUG_INFO      1
#define FONT_SIZE       16

/* os-specific font fallbacks */
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

/* undo history depth (ctrl+z) */
#define MAX_HISTORY_SIZE 100

/* default view */
#define INITIAL_CENTER_RE   -0.5
#define INITIAL_CENTER_IM    0.0
#define INITIAL_ZOOM         3.0

#endif /* CONFIG_H */
