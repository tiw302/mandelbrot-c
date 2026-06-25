#include "ini_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum { TYPE_INT, TYPE_DOUBLE } ConfigType;

typedef struct {
    const char* key;
    void* target;
    ConfigType type;
} ConfigEntry;

static ConfigEntry config_table[] = {
    {"window_width", &current_window_width, TYPE_INT},
    {"window_height", &current_window_height, TYPE_INT},
    {"default_iterations", &current_default_iterations, TYPE_INT},
    {"max_iterations_limit", &current_max_iterations_limit, TYPE_INT},
    {"escape_radius", &current_escape_radius, TYPE_DOUBLE},
    {"default_thread_count", &current_default_thread_count, TYPE_INT},
    {"default_palette", &current_default_palette, TYPE_INT},
};

// helper to trim whitespace from a string
static char* trim_whitespace(char* str) {
    char* end;

    // trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

int load_config_from_file(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        return 0;  // file not found or cannot be opened
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim_whitespace(line);

        // skip empty lines and comments
        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#' || trimmed_line[0] == ';') {
            continue;
        }

        // parse key-value pairs
        char* equals_sign = strchr(trimmed_line, '=');
        if (equals_sign) {
            *equals_sign = '\0';
            char* key = trim_whitespace(trimmed_line);
            char* value = trim_whitespace(equals_sign + 1);

            for (size_t i = 0; i < sizeof(config_table) / sizeof(config_table[0]); i++) {
                if (strcmp(key, config_table[i].key) == 0) {
                    if (config_table[i].type == TYPE_INT) {
                        *((int*)config_table[i].target) = atoi(value);
                    } else if (config_table[i].type == TYPE_DOUBLE) {
                        *((double*)config_table[i].target) = atof(value);
                    }
                    break;
                }
            }
        }
    }

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
    if (current_default_palette >= pal_count) current_default_palette = pal_count - 1;

    // clamp escape radius to positive and reasonable range
    if (current_escape_radius <= 0.0) current_escape_radius = ESCAPE_RADIUS;
    if (current_escape_radius > 1000.0) current_escape_radius = 1000.0;

    fclose(file);
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
