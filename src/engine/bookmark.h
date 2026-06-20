#ifndef BOOKMARK_H
#define BOOKMARK_H

#include "config.h"
#include "core_math.h"

typedef struct {
    double center_re;
    double center_im;
    double zoom;
    int max_iterations;
    int fractal_type;  // 0=mandelbrot, 1=julia, 2=burning ship
    complex_t julia_c;
} Bookmark;

// saves a bookmark to bookmarks.json in the current directory
void save_bookmark(const Bookmark* b);

/* loads a bookmark from bookmarks.json by index.
 * returns 1 if successful, 0 if out of bounds or file doesn't exist. */
int load_bookmark(int index, Bookmark* b);

// returns the total number of bookmarks saved
int get_bookmark_count(void);

#endif
