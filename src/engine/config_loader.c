/* config_loader.c
 *
 * parser for loading startup settings from json configuration files.
 * reads window size, default iterations, and limits on launch.
 */

#include "config_loader.h"

#include <stdio.h>
#include <stdlib.h>

#include "cjsonx.h"
#include "color.h"
#include "config.h"

// default values populated from config.h
static int current_window_width = WINDOW_WIDTH;
static int current_window_height = WINDOW_HEIGHT;
static int current_default_iterations = DEFAULT_ITERATIONS;
static int current_max_iterations_limit = MAX_ITERATIONS_LIMIT;
static double current_escape_radius = ESCAPE_RADIUS;
static int current_default_thread_count = DEFAULT_THREAD_COUNT;
static int current_default_palette = DEFAULT_PALETTE;

int load_config_from_file(const char* filepath) {
    cjsonx_doc_t* doc = cjsonx_read_file(filepath);
    if (!doc) {
        return 0;  // file not found or cannot be opened
    }

    cjsonx_val_t root = doc->root;
    if (cjsonx_get_type(root) != CJSONX_ARRAY && cjsonx_get_type(root) != CJSONX_OBJECT) {
        cjsonx_doc_free(doc);
        return 0;
    }

    cjsonx_val_t w = cjsonx_get(root, "window_width");
    cjsonx_val_t h = cjsonx_get(root, "window_height");
    cjsonx_val_t di = cjsonx_get(root, "default_iterations");
    cjsonx_val_t mil = cjsonx_get(root, "max_iterations_limit");
    cjsonx_val_t er = cjsonx_get(root, "escape_radius");
    cjsonx_val_t dtc = cjsonx_get(root, "default_thread_count");
    cjsonx_val_t dp = cjsonx_get(root, "default_palette");

    if (cjsonx_get_type(w) == CJSONX_NUMBER) current_window_width = (int)cjsonx_num(w);
    if (cjsonx_get_type(h) == CJSONX_NUMBER) current_window_height = (int)cjsonx_num(h);
    if (cjsonx_get_type(di) == CJSONX_NUMBER) current_default_iterations = (int)cjsonx_num(di);
    if (cjsonx_get_type(mil) == CJSONX_NUMBER) current_max_iterations_limit = (int)cjsonx_num(mil);
    /* escape_radius is parsed and stored but not yet wired into the render kernels.
     * kernels still use the ESCAPE_RADIUS compile-time macro from config.h.
     * setting this in settings.json has no visible effect on rendering until
     * get_config_escape_radius() is plumbed through to RenderJob and the kernels. */
    if (cjsonx_get_type(er) == CJSONX_NUMBER) current_escape_radius = cjsonx_num(er);
    if (cjsonx_get_type(dtc) == CJSONX_NUMBER) current_default_thread_count = (int)cjsonx_num(dtc);
    if (cjsonx_get_type(dp) == CJSONX_NUMBER) current_default_palette = (int)cjsonx_num(dp);

    cjsonx_doc_free(doc);

    // clamp parsed parameters to safe bounds
    if (current_max_iterations_limit < 10) current_max_iterations_limit = 10;
    if (current_max_iterations_limit > 100000) current_max_iterations_limit = 100000;

    if (current_window_width < 320) current_window_width = 320;
    if (current_window_width > 8192) current_window_width = 8192;

    if (current_window_height < 240) current_window_height = 240;
    if (current_window_height > 8192) current_window_height = 8192;

    if (current_default_iterations < 10) current_default_iterations = 10;
    if (current_default_iterations > current_max_iterations_limit)
        current_default_iterations = current_max_iterations_limit;

    if (current_default_thread_count < 0) current_default_thread_count = 0;
    if (current_default_thread_count > 64) current_default_thread_count = 64;

    int pal_count = get_palette_count();
    if (current_default_palette < 0) current_default_palette = 0;
    if (pal_count > 0 && current_default_palette >= pal_count)
        current_default_palette = pal_count - 1;

    // clamp escape radius to positive and reasonable range
    if (current_escape_radius <= 0.0) current_escape_radius = ESCAPE_RADIUS;
    if (current_escape_radius > 1000.0) current_escape_radius = 1000.0;

    return 1;
}

int get_config_window_width(void) {
    return current_window_width;
}
int get_config_window_height(void) {
    return current_window_height;
}
int get_config_default_iterations(void) {
    return current_default_iterations;
}
int get_config_max_iterations_limit(void) {
    return current_max_iterations_limit;
}
double get_config_escape_radius(void) {
    return current_escape_radius;
}
int get_config_default_thread_count(void) {
    return current_default_thread_count;
}
int get_config_default_palette(void) {
    return current_default_palette;
}
