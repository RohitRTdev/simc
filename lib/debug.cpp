#include <spdlog/spdlog.h>
#include <memory>
#include "dll.h"

std::shared_ptr<spdlog::logger> sim_logger;

//This file is only included during a debug build.
//It initializes the debugger and allows the library to use our module logger.
//This gets called by entry.cpp during initialization
DLL_ATTRIB void init_debugger(std::shared_ptr<spdlog::logger>& logger) {
    sim_logger = logger;
}