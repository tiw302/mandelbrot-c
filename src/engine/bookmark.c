#include "bookmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOKMARKS_FILE "bookmarks.json"
#define MAX_BOOKMARKS 1024

/* very simple json writer to avoid pulling in a full json library for just one feature */
void save_bookmark(const Bookmark* b) {
    FILE* f = fopen(BOOKMARKS_FILE, "a+");
    if (!f) return;

    /* check if file is empty */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size == 0) {
        fprintf(f, "[\n");
    } else {
        /* scan backwards for the closing ']' — handles crlf line endings
         * and any trailing whitespace after manual edits. */
        long pos = size;
        int found = 0;
        while (pos > 0) {
            fseek(f, --pos, SEEK_SET);
            int ch = fgetc(f);
            if (ch == ']') {
                fseek(f, pos, SEEK_SET);
                fprintf(f, ",\n");
                found = 1;
                break;
            }
        }
        if (!found) {
            fclose(f);
            return;
        }
    }

    fprintf(f, "  {\n");
    fprintf(f, "    \"center_re\": %.15g,\n", b->center_re);
    fprintf(f, "    \"center_im\": %.15g,\n", b->center_im);
    fprintf(f, "    \"zoom\": %.15g,\n", b->zoom);
    fprintf(f, "    \"max_iterations\": %d,\n", b->max_iterations);
    fprintf(f, "    \"fractal_type\": %d,\n", b->fractal_type);
    fprintf(f, "    \"julia_c_re\": %.15g,\n", b->julia_c.re);
    fprintf(f, "    \"julia_c_im\": %.15g\n", b->julia_c.im);
    fprintf(f, "  }\n]");

    fclose(f);
}

/* simple scanner for the json structure we write */
static int scan_bookmarks(Bookmark* bookmarks, int max_count) {
    FILE* f = fopen(BOOKMARKS_FILE, "r");
    if (!f) return 0;

    int count = 0;
    char line[256];
    Bookmark current = {0};
    int in_object = 0;

    while (fgets(line, sizeof(line), f) && count < max_count) {
        if (strstr(line, "{")) {
            in_object = 1;
        } else if (strstr(line, "}")) {
            if (in_object) {
                bookmarks[count++] = current;
                in_object = 0;
                memset(&current, 0, sizeof(Bookmark));
            }
        } else if (in_object) {
            sscanf(line, " \"center_re\": %lf,", &current.center_re);
            sscanf(line, " \"center_im\": %lf,", &current.center_im);
            sscanf(line, " \"zoom\": %lf,", &current.zoom);
            sscanf(line, " \"max_iterations\": %d,", &current.max_iterations);
            sscanf(line, " \"fractal_type\": %d,", &current.fractal_type);
            sscanf(line, " \"julia_c_re\": %lf,", &current.julia_c.re);
            sscanf(line, " \"julia_c_im\": %lf", &current.julia_c.im);
        }
    }

    fclose(f);
    return count;
}

int load_bookmark(int index, Bookmark* b) {
    Bookmark bookmarks[MAX_BOOKMARKS];
    int count = scan_bookmarks(bookmarks, MAX_BOOKMARKS);
    
    if (index >= 0 && index < count) {
        *b = bookmarks[index];
        return 1;
    }
    return 0;
}

/* lightweight counter — avoids deserializing all bookmarks onto the stack
 * just to return a count. scans for closing braces which delimit objects. */
int get_bookmark_count(void) {
    FILE* f = fopen(BOOKMARKS_FILE, "r");
    if (!f) return 0;
    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), f))
        if (strstr(line, "}")) count++;
    fclose(f);
    return count;
}
