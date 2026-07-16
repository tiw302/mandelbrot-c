/* test_config.c
 *
 * unit tests for ini configuration parser.
 * validates loading, key-value parsing, and range clamping limits.
 */
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "config_loader.h"

#define TEST_START(name) printf("running test: %s... ", name);
#define TEST_END() printf("passed!\n");
#define EXPECT(cond)                                                          \
    if (!(cond)) {                                                            \
        fprintf(stderr, "\\ntest failed: %s at line %d\\n", #cond, __LINE__); \
        exit(1);                                                              \
    }

/* 
 * [TEST CASE] config defaults
 * tests the functionality of config defaults.
 */
void test_config_defaults(void) {
    TEST_START("config defaults & clamping");

    // we will just read whatever is in settings.json (or recreate it if missing)
    load_config_from_file("settings.json");


    // check bounds enforcement
    int threads = get_config_default_thread_count();
    EXPECT(threads >= 0 && threads <= 64);

    int iters = get_config_default_iterations();
    EXPECT(iters >= 10 && iters <= get_config_max_iterations_limit());

    int pal = get_config_default_palette();
    EXPECT(pal >= 0 && pal < 10);

    TEST_END();
}

int main(void) {
    printf("--- starting config tests ---\\n");
    test_config_defaults();
    printf("--- config tests passed! ---\\n");
    return 0;
}
