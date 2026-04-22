#ifndef CONFIG_H
#define CONFIG_H

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

#define DEFAULT_ITERATIONS 500
#define MAX_ITERATIONS_LIMIT 10000

#define ESCAPE_RADIUS 10

#define DEFAULT_THREAD_COUNT 4

#define DEBUG_INFO 1
#define FONT_SIZE 16

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

#define MAX_HISTORY_SIZE 100

#define INITIAL_CENTER_RE -0.5
#define INITIAL_CENTER_IM 0.0
#define INITIAL_ZOOM 3.0

typedef struct {
    double center_re;
    double center_im;
    double zoom;
} ViewState;

#endif