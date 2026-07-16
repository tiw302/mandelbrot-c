/* bookmark.c
 *
 * handles serialization, deserialization, and persistent storage of bookmark states.
 * uses json format to save complex-plane viewports, iterations, and render parameters.
 */

#include "bookmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjsonx.h"

static char bookmarks_file_path[256] = "bookmarks.json";

static Bookmark* bookmark_cache = NULL;
static int bookmark_cache_capacity = 0;
static int bookmark_cache_count = -1; // -1 means stale/unloaded

void bookmark_cache_free(void) {
    if (bookmark_cache) {
        free(bookmark_cache);
        bookmark_cache = NULL;
    }
    bookmark_cache_capacity = 0;
    bookmark_cache_count = -1;
}

static void bookmark_cache_load(void);

// sets the active bookmark filename
void set_bookmarks_file(const char* filepath) {
    if (filepath) {
        strncpy(bookmarks_file_path, filepath, sizeof(bookmarks_file_path) - 1);
        bookmarks_file_path[sizeof(bookmarks_file_path) - 1] = '\0';
    }
}


// simple json writer that rewrites the whole file using cJSON
void save_bookmark(const Bookmark* b) {
    bookmark_cache_load();

    if (bookmark_cache_count >= bookmark_cache_capacity) {
        int new_cap = (bookmark_cache_capacity == 0) ? 16 : bookmark_cache_capacity * 2;
        Bookmark* new_cache = realloc(bookmark_cache, new_cap * sizeof(Bookmark));
        if (!new_cache) return;
        bookmark_cache = new_cache;
        bookmark_cache_capacity = new_cap;
    }
    bookmark_cache[bookmark_cache_count++] = *b;

    cjsonx_doc_t* doc = cjsonx_doc_new();
    if (!doc) return;

    cjsonx_val_t root = cjsonx_create_array(doc);

    for (int i = 0; i < bookmark_cache_count; i++) {
        cjsonx_val_t obj = cjsonx_create_object(doc);
        cjsonx_object_set(obj, "name", cjsonx_create_string(doc, bookmark_cache[i].name));
        cjsonx_object_set(obj, "center_re", cjsonx_create_number(doc, bookmark_cache[i].center_re));
        cjsonx_object_set(obj, "center_im", cjsonx_create_number(doc, bookmark_cache[i].center_im));
        cjsonx_object_set(obj, "zoom", cjsonx_create_number(doc, bookmark_cache[i].zoom));
        cjsonx_object_set(obj, "max_iterations",
                          cjsonx_create_number(doc, bookmark_cache[i].max_iterations));
        cjsonx_object_set(obj, "fractal_type",
                          cjsonx_create_number(doc, bookmark_cache[i].fractal_type));
        cjsonx_object_set(obj, "julia_c_re", cjsonx_create_number(doc, bookmark_cache[i].julia_c.re));
        cjsonx_object_set(obj, "julia_c_im", cjsonx_create_number(doc, bookmark_cache[i].julia_c.im));
        cjsonx_array_push(root, obj);
    }

    doc->root = root;
    char* json_string = cjsonx_stringify(doc);
    cjsonx_doc_free(doc);

    if (!json_string) {
        return;
    }

    char tmp_file_path[512];
    snprintf(tmp_file_path, sizeof(tmp_file_path), "%s.tmp", bookmarks_file_path);
    FILE* f = fopen(tmp_file_path, "w");
    if (!f) {
        free(json_string);
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
}

// robust scanner using cJSON
static void bookmark_cache_load(void) {
    if (bookmark_cache_count >= 0) return; // already fresh

    cjsonx_doc_t* doc = cjsonx_read_file(bookmarks_file_path);
    if (!doc) {
        bookmark_cache_count = 0;
        return;
    }

    cjsonx_val_t root = doc->root;
    if (cjsonx_get_type(root) != CJSONX_ARRAY) {
        cjsonx_doc_free(doc);
        bookmark_cache_count = 0;
        return;
    }

    int count = 0;
    cjsonx_iter_t it = cjsonx_iter_init(root);
    while (cjsonx_iter_next(&it)) {
        if (count >= bookmark_cache_capacity) {
            int new_cap = (bookmark_cache_capacity == 0) ? 16 : bookmark_cache_capacity * 2;
            Bookmark* new_cache = realloc(bookmark_cache, new_cap * sizeof(Bookmark));
            if (!new_cache) break; // out of memory
            bookmark_cache = new_cache;
            bookmark_cache_capacity = new_cap;
        }

        cjsonx_val_t item = it.value;

        cjsonx_val_t name = cjsonx_get(item, "name");
        cjsonx_val_t cre = cjsonx_get(item, "center_re");
        cjsonx_val_t cim = cjsonx_get(item, "center_im");
        cjsonx_val_t z = cjsonx_get(item, "zoom");
        cjsonx_val_t mi = cjsonx_get(item, "max_iterations");
        cjsonx_val_t ft = cjsonx_get(item, "fractal_type");
        cjsonx_val_t jre = cjsonx_get(item, "julia_c_re");
        cjsonx_val_t jim = cjsonx_get(item, "julia_c_im");

        memset(bookmark_cache[count].name, 0, sizeof(bookmark_cache[count].name));
        if (cjsonx_get_type(name) == CJSONX_STRING) {
            strncpy(bookmark_cache[count].name, cjsonx_str(name), sizeof(bookmark_cache[count].name) - 1);
        } else {
            snprintf(bookmark_cache[count].name, sizeof(bookmark_cache[count].name), "Bookmark #%d", count + 1);
        }

        if (cjsonx_get_type(cre) == CJSONX_NUMBER) bookmark_cache[count].center_re = cjsonx_num(cre);
        if (cjsonx_get_type(cim) == CJSONX_NUMBER) bookmark_cache[count].center_im = cjsonx_num(cim);
        if (cjsonx_get_type(z) == CJSONX_NUMBER) bookmark_cache[count].zoom = cjsonx_num(z);
        if (cjsonx_get_type(mi) == CJSONX_NUMBER) bookmark_cache[count].max_iterations = cjsonx_int(mi);
        if (cjsonx_get_type(ft) == CJSONX_NUMBER) bookmark_cache[count].fractal_type = cjsonx_int(ft);
        if (cjsonx_get_type(jre) == CJSONX_NUMBER) bookmark_cache[count].julia_c.re = cjsonx_num(jre);
        if (cjsonx_get_type(jim) == CJSONX_NUMBER) bookmark_cache[count].julia_c.im = cjsonx_num(jim);

        count++;
    }

    cjsonx_doc_free(doc);
    bookmark_cache_count = count;
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
