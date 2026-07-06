#include "app_runner.h"

sapp_desc sokol_main(int argc, char* argv[]) {
    return app_runner_get_desc(APP_BACKEND_VIDEO);
}
