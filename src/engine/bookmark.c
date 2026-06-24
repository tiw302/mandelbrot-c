#include "bookmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

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

// simple json writer that rewrites the whole file using cJSON
void save_bookmark(const Bookmark* b) {
    Bookmark* bookmarks = (Bookmark*)malloc(sizeof(Bookmark) * MAX_BOOKMARKS);
    if (!bookmarks) return;

    int count = scan_bookmarks(bookmarks, MAX_BOOKMARKS);
    if (count < MAX_BOOKMARKS) {
        bookmarks[count++] = *b;
    }

    cjsonx_doc_t* doc = cjsonx_doc_new();
    if (!doc) {
        free(bookmarks);
        return;
    }
    
    cjsonx_val_t root = cjsonx_create_array(doc);

    for (int i = 0; i < count; i++) {
        cjsonx_val_t obj = cjsonx_create_object(doc);
        cjsonx_object_set(obj, "center_re", cjsonx_create_number(doc, bookmarks[i].center_re));
        cjsonx_object_set(obj, "center_im", cjsonx_create_number(doc, bookmarks[i].center_im));
        cjsonx_object_set(obj, "zoom", cjsonx_create_number(doc, bookmarks[i].zoom));
        cjsonx_object_set(obj, "max_iterations", cjsonx_create_number(doc, bookmarks[i].max_iterations));
        cjsonx_object_set(obj, "fractal_type", cjsonx_create_number(doc, bookmarks[i].fractal_type));
        cjsonx_object_set(obj, "julia_c_re", cjsonx_create_number(doc, bookmarks[i].julia_c.re));
        cjsonx_object_set(obj, "julia_c_im", cjsonx_create_number(doc, bookmarks[i].julia_c.im));
        cjsonx_array_push(root, obj);
    }

    doc->root = root;
    char* json_string = cjsonx_stringify(doc);
    cjsonx_doc_free(doc);

    if (!json_string) {
        free(bookmarks);
        return;
    }

    char tmp_file_path[512];
    snprintf(tmp_file_path, sizeof(tmp_file_path), "%s.tmp", bookmarks_file_path);
    FILE* f = fopen(tmp_file_path, "w");
    if (!f) {
        free(json_string);
        free(bookmarks);
        return;
    }

    fprintf(f, "%s\n", json_string);
    free(json_string);

    // ensure durable write
    fflush(f);
    fclose(f);
    
    /* write to a temporary file first, then atomically rename to prevent
     * data corruption if the program crashes midway through writing. */
    rename(tmp_file_path, bookmarks_file_path);

    free(bookmarks);
}

// robust scanner using cJSON
static int scan_bookmarks(Bookmark* bookmarks, int max_count) {
    cjsonx_doc_t* doc = cjsonx_read_file(bookmarks_file_path);
    if (!doc) return 0;

    cjsonx_val_t root = doc->root;
    if (cjsonx_get_type(root) != CJSONX_ARRAY) {
        cjsonx_doc_free(doc);
        return 0;
    }

    int count = 0;
    cjsonx_iter_t it = cjsonx_iter_init(root);
    while (cjsonx_iter_next(&it)) {
        if (count >= max_count) break;
        
        cjsonx_val_t item = it.value;

        cjsonx_val_t cre = cjsonx_get(item, "center_re");
        cjsonx_val_t cim = cjsonx_get(item, "center_im");
        cjsonx_val_t z = cjsonx_get(item, "zoom");
        cjsonx_val_t mi = cjsonx_get(item, "max_iterations");
        cjsonx_val_t ft = cjsonx_get(item, "fractal_type");
        cjsonx_val_t jre = cjsonx_get(item, "julia_c_re");
        cjsonx_val_t jim = cjsonx_get(item, "julia_c_im");

        if (cjsonx_get_type(cre) == CJSONX_NUMBER) bookmarks[count].center_re = cjsonx_num(cre);
        if (cjsonx_get_type(cim) == CJSONX_NUMBER) bookmarks[count].center_im = cjsonx_num(cim);
        if (cjsonx_get_type(z) == CJSONX_NUMBER) bookmarks[count].zoom = cjsonx_num(z);
        if (cjsonx_get_type(mi) == CJSONX_NUMBER) bookmarks[count].max_iterations = cjsonx_int(mi);
        if (cjsonx_get_type(ft) == CJSONX_NUMBER) bookmarks[count].fractal_type = cjsonx_int(ft);
        if (cjsonx_get_type(jre) == CJSONX_NUMBER) bookmarks[count].julia_c.re = cjsonx_num(jre);
        if (cjsonx_get_type(jim) == CJSONX_NUMBER) bookmarks[count].julia_c.im = cjsonx_num(jim);

        count++;
    }

    cjsonx_doc_free(doc);
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
