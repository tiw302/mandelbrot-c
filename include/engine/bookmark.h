/* bookmark.h
 *
 * camera bookmarking and saving functionality.
 */

#ifndef BOOKMARK_H
#define BOOKMARK_H

#include "config.h"
#include "core_math.h"

typedef struct {
    char name[64];
    double center_re;
    double center_im;
    double zoom;
    int max_iterations;
    int fractal_type;  // 0=mandelbrot, 1=julia, 2=burning ship
    complex_t julia_c;
} Bookmark;

// configures the target file for saving/loading bookmarks
void set_bookmarks_file(const char* filepath);

void save_bookmark(const Bookmark* b);

/* loads a bookmark from bookmarks.json by index.
 * returns 1 if successful, 0 if out of bounds or file doesn't exist. */
int load_bookmark(int index, Bookmark* b);

// deletes a bookmark from the file by index
void delete_bookmark(int index);

// returns the total number of bookmarks saved
int get_bookmark_count(void);

// returns a pointer to the internal array of bookmarks
const Bookmark* get_bookmarks_array(int* out_count);

// frees the bookmark cache
void bookmark_cache_free(void);

#endif  // BOOKMARK_H
