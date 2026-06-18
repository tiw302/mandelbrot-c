/* ini_config.h
 *
 * handles persistent configuration loading from settings.txt.
 * provides a read-only api to retrieve validated parameters.
 */

#ifndef INI_CONFIG_H
#define INI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// loads configuration from disk. returns 1 on success, 0 on failure.
int load_config_from_file(const char* filepath);

/* config getters — return validated values.
 * if no file was loaded, these return defaults from config.h. */

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

#endif
