#ifndef INI_CONFIG_H
#define INI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * loads configuration from a file.
 * returns 1 on success, 0 on failure (e.g., file not found).
 */
int load_config_from_file(const char* filepath);

/* 
 * retrieves the loaded configuration values.
 * values are initialized with defaults from config.h.
 */
int get_config_window_width(void);
int get_config_window_height(void);
int get_config_default_iterations(void);
int get_config_max_iterations_limit(void);
double get_config_escape_radius(void);
int get_config_default_thread_count(void);
int get_config_default_palette(void);

#ifdef __cplusplus
}
#endif

#endif /* INI_CONFIG_H */
