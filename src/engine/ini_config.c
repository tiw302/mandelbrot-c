#include "ini_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

// default values populated from config.h
static int current_window_width = WINDOW_WIDTH;
static int current_window_height = WINDOW_HEIGHT;
static int current_default_iterations = DEFAULT_ITERATIONS;
static int current_max_iterations_limit = MAX_ITERATIONS_LIMIT;
static double current_escape_radius = ESCAPE_RADIUS;
static int current_default_thread_count = DEFAULT_THREAD_COUNT;
static int current_default_palette = DEFAULT_PALETTE;

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

            if (strcmp(key, "window_width") == 0) {
                current_window_width = atoi(value);
            } else if (strcmp(key, "window_height") == 0) {
                current_window_height = atoi(value);
            } else if (strcmp(key, "default_iterations") == 0) {
                current_default_iterations = atoi(value);
            } else if (strcmp(key, "max_iterations_limit") == 0) {
                current_max_iterations_limit = atoi(value);
            } else if (strcmp(key, "escape_radius") == 0) {
                current_escape_radius = atof(value);
            } else if (strcmp(key, "default_thread_count") == 0) {
                current_default_thread_count = atoi(value);
            } else if (strcmp(key, "default_palette") == 0) {
                current_default_palette = atoi(value);
            }
        }
    }

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
