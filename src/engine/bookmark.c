/* bookmark.c
 *
 * handles serialization, deserialization, and persistent storage of bookmark states.
 * uses JSON format to save complex-plane viewports, iterations, and render parameters.
 */

#include "bookmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

static char bookmarks_file_path[256] = "bookmarks.json";
#define MAX_BOOKMARKS 1024

static int scan_bookmarks(Bookmark* bookmarks, int max_count);

/* simple in-memory cache to avoid reading and parsing the json file on every
 * call to get_bookmark_count() or load_bookmark(). invalidated by save_bookmark(). */
static Bookmark bookmark_cache[MAX_BOOKMARKS];
static int bookmark_cache_count = -1; // -1 means stale/unloaded

static void bookmark_cache_invalidate(void) {
    bookmark_cache_count = -1;
}

static void bookmark_cache_load(void) {
    if (bookmark_cache_count >= 0) return; // already fresh
    bookmark_cache_count = scan_bookmarks(bookmark_cache, MAX_BOOKMARKS);
}

// sets the active bookmark filename
void set_bookmarks_file(const char* filepath) {
    if (filepath) {
        strncpy(bookmarks_file_path, filepath, sizeof(bookmarks_file_path) - 1);
        bookmarks_file_path[sizeof(bookmarks_file_path) - 1] = '\0';
    }
}


// simple json writer that rewrites the whole file using cJSON
void save_bookmark(const Bookmark* b) {
    bookmark_cache_invalidate();

    Bookmark* bookmarks = (Bookmark*)calloc(MAX_BOOKMARKS, sizeof(Bookmark));
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
        cjsonx_object_set(obj, "name", cjsonx_create_string(doc, bookmarks[i].name));
        cjsonx_object_set(obj, "center_re", cjsonx_create_number(doc, bookmarks[i].center_re));
        cjsonx_object_set(obj, "center_im", cjsonx_create_number(doc, bookmarks[i].center_im));
        cjsonx_object_set(obj, "zoom", cjsonx_create_number(doc, bookmarks[i].zoom));
        cjsonx_object_set(obj, "max_iterations",
                          cjsonx_create_number(doc, bookmarks[i].max_iterations));
        cjsonx_object_set(obj, "fractal_type",
                          cjsonx_create_number(doc, bookmarks[i].fractal_type));
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
#ifdef _WIN32
    /* on windows rename fails if destination already exists */
    remove(bookmarks_file_path);
#endif
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

        cjsonx_val_t name = cjsonx_get(item, "name");
        cjsonx_val_t cre = cjsonx_get(item, "center_re");
        cjsonx_val_t cim = cjsonx_get(item, "center_im");
        cjsonx_val_t z = cjsonx_get(item, "zoom");
        cjsonx_val_t mi = cjsonx_get(item, "max_iterations");
        cjsonx_val_t ft = cjsonx_get(item, "fractal_type");
        cjsonx_val_t jre = cjsonx_get(item, "julia_c_re");
        cjsonx_val_t jim = cjsonx_get(item, "julia_c_im");

        memset(bookmarks[count].name, 0, sizeof(bookmarks[count].name));
        if (cjsonx_get_type(name) == CJSONX_STRING) {
            strncpy(bookmarks[count].name, cjsonx_str(name), sizeof(bookmarks[count].name) - 1);
        } else {
            snprintf(bookmarks[count].name, sizeof(bookmarks[count].name), "Bookmark #%d", count + 1);
        }

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
    bookmark_cache_load();
    if (index >= 0 && index < bookmark_cache_count) {
        *b = bookmark_cache[index];
        return 1;
    }
    return 0;
}

// returns the count of correctly formatted bookmark objects
int get_bookmark_count(void) {
    bookmark_cache_load();
    return bookmark_cache_count;
}

const Bookmark* get_bookmarks_array(int* out_count) {
    bookmark_cache_load();
    if (out_count) *out_count = bookmark_cache_count;
    return bookmark_cache;
}

void delete_bookmark(int index) {
    bookmark_cache_load();
    if (index < 0 || index >= bookmark_cache_count) return;

    // Shift everything down
    for (int i = index; i < bookmark_cache_count - 1; i++) {
        bookmark_cache[i] = bookmark_cache[i + 1];
    }
    bookmark_cache_count--;

    // Re-save entire file from cache
    cjsonx_doc_t* doc = cjsonx_doc_new();
    if (!doc) return;

    cjsonx_val_t root = cjsonx_create_array(doc);
    for (int i = 0; i < bookmark_cache_count; i++) {
        cjsonx_val_t obj = cjsonx_create_object(doc);
        cjsonx_object_set(obj, "name", cjsonx_create_string(doc, bookmark_cache[i].name));
        cjsonx_object_set(obj, "center_re", cjsonx_create_number(doc, bookmark_cache[i].center_re));
        cjsonx_object_set(obj, "center_im", cjsonx_create_number(doc, bookmark_cache[i].center_im));
        cjsonx_object_set(obj, "zoom", cjsonx_create_number(doc, bookmark_cache[i].zoom));
        cjsonx_object_set(obj, "max_iterations", cjsonx_create_number(doc, bookmark_cache[i].max_iterations));
        cjsonx_object_set(obj, "fractal_type", cjsonx_create_number(doc, bookmark_cache[i].fractal_type));
        cjsonx_object_set(obj, "julia_c_re", cjsonx_create_number(doc, bookmark_cache[i].julia_c.re));
        cjsonx_object_set(obj, "julia_c_im", cjsonx_create_number(doc, bookmark_cache[i].julia_c.im));
        cjsonx_array_push(root, obj);
    }
    doc->root = root;

    char* json_string = cjsonx_stringify(doc);
    cjsonx_doc_free(doc);
    if (!json_string) return;

    char tmp_file_path[512];
    snprintf(tmp_file_path, sizeof(tmp_file_path), "%s.tmp", bookmarks_file_path);
    FILE* f = fopen(tmp_file_path, "w");
    if (!f) {
        free(json_string);
        return;
    }
    fprintf(f, "%s\n", json_string);
    free(json_string);
    fflush(f);
    fclose(f);
    rename(tmp_file_path, bookmarks_file_path);
}
