#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

int load_config_from_file(const char* filepath);
int get_config_window_width(void);
int get_config_window_height(void);
int get_config_default_iterations(void);
int get_config_max_iterations_limit(void);
double get_config_escape_radius(void);
int get_config_default_thread_count(void);
int get_config_default_palette(void);

#endif
