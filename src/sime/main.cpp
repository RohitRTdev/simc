#include <iostream>
#include "debug-api.h"

int app_start(int argc, char** argv) {
    sim_log_info("Starting preprocessor...");
    sim_log_warn("Warn test.. pc");
    sim_log_debug("Debug test... pc");

    return 0;
}