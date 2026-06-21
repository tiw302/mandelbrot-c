#include "bookmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char bookmarks_file_path[256] = "bookmarks.json";
#define MAX_BOOKMARKS 1024

// sets the active bookmark filename
void set_bookmarks_file(const char* filepath) {
    if (filepath) {
        strncpy(bookmarks_file_path, filepath, sizeof(bookmarks_file_path) - 1);
        bookmarks_file_path[sizeof(bookmarks_file_path) - 1] = '\0';
    }
}

static int scan_bookmarks(Bookmark* bookmarks, int max_count);

// simple json writer that rewrites the whole file to avoid partial write corruption
void save_bookmark(const Bookmark* b) {
    Bookmark* bookmarks = (Bookmark*)malloc(sizeof(Bookmark) * MAX_BOOKMARKS);
    if (!bookmarks) return;

    int count = scan_bookmarks(bookmarks, MAX_BOOKMARKS);
    if (count < MAX_BOOKMARKS) {
        bookmarks[count++] = *b;
    }

    FILE* f = fopen(bookmarks_file_path, "w");
    if (!f) {
        free(bookmarks);
        return;
    }

    fprintf(f, "[\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "  {\n");
        fprintf(f, "    \"center_re\": %.15g,\n", bookmarks[i].center_re);
        fprintf(f, "    \"center_im\": %.15g,\n", bookmarks[i].center_im);
        fprintf(f, "    \"zoom\": %.15g,\n", bookmarks[i].zoom);
        fprintf(f, "    \"max_iterations\": %d,\n", bookmarks[i].max_iterations);
        fprintf(f, "    \"fractal_type\": %d,\n", bookmarks[i].fractal_type);
        fprintf(f, "    \"julia_c_re\": %.15g,\n", bookmarks[i].julia_c.re);
        fprintf(f, "    \"julia_c_im\": %.15g\n", bookmarks[i].julia_c.im);
        if (i < count - 1) {
            fprintf(f, "  },\n");
        } else {
            fprintf(f, "  }\n");
        }
    }
    fprintf(f, "]\n");

    // ensure durable write
    fflush(f);
    fclose(f);
    free(bookmarks);
}

// simple scanner for the json structure we write
static int scan_bookmarks(Bookmark* bookmarks, int max_count) {
    FILE* f = fopen(bookmarks_file_path, "r");
    if (!f) return 0;

    int count = 0;
    char line[512];
    Bookmark current = {0};
    int in_object = 0;

    while (fgets(line, sizeof(line), f) && count < max_count) {
        if (strstr(line, "{")) {
            in_object = 1;
        }
        if (in_object) {
            char* p;
            char* colon;
            if ((p = strstr(line, "\"center_re\"")) != NULL && (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%lf", &current.center_re);
            }
            if ((p = strstr(line, "\"center_im\"")) != NULL && (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%lf", &current.center_im);
            }
            if ((p = strstr(line, "\"zoom\"")) != NULL && (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%lf", &current.zoom);
            }
            if ((p = strstr(line, "\"max_iterations\"")) != NULL &&
                (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%d", &current.max_iterations);
            }
            if ((p = strstr(line, "\"fractal_type\"")) != NULL &&
                (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%d", &current.fractal_type);
            }
            if ((p = strstr(line, "\"julia_c_re\"")) != NULL && (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%lf", &current.julia_c.re);
            }
            if ((p = strstr(line, "\"julia_c_im\"")) != NULL && (colon = strchr(p, ':')) != NULL) {
                sscanf(colon + 1, "%lf", &current.julia_c.im);
            }
        }
        if (strstr(line, "}")) {
            if (in_object) {
                bookmarks[count++] = current;
                in_object = 0;
                memset(&current, 0, sizeof(Bookmark));
            }
        }
    }

    fclose(f);
    return count;
}

int load_bookmark(int index, Bookmark* b) {
    Bookmark* bookmarks = (Bookmark*)malloc(sizeof(Bookmark) * MAX_BOOKMARKS);
    if (!bookmarks) return 0;

    int count = scan_bookmarks(bookmarks, MAX_BOOKMARKS);
    int success = 0;

    if (index >= 0 && index < count) {
        *b = bookmarks[index];
        success = 1;
    }

    free(bookmarks);
    return success;
}

// scans the file and returns the actual count of correctly formatted objects
int get_bookmark_count(void) {
    Bookmark* bookmarks = (Bookmark*)malloc(sizeof(Bookmark) * MAX_BOOKMARKS);
    if (!bookmarks) return 0;

    int count = scan_bookmarks(bookmarks, MAX_BOOKMARKS);
    free(bookmarks);

    return count;
}
