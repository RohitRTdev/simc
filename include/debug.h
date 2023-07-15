#pragma once

#include <iostream>
#include <memory>
#include <ctime>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/fmt/chrono.h"
#include "dll.h"
#define LOGGER "sim-logger"

std::shared_ptr<spdlog::logger> sim_logger;

enum log_lvls {
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG
};

static std::string get_file_timestamp() {
    std::time_t t = std::time(nullptr);

    return fmt::format("{:%Y-%m-%d}.{:%H_%M_%S}", fmt::localtime(t), fmt::localtime(t));
}

//Create a logger which outputs to console and a file
static inline void init_logger() {

    std::string log_file_name = fmt::format("{}-{}.txt", LOGGER, get_file_timestamp());
    const std::string log_file = fmt::format("{}/{}/{}", LOG_DIR, MODULENAME, log_file_name);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_st>());
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true));

    auto logger = std::make_shared<spdlog::logger>(LOGGER, std::begin(sinks), std::end(sinks));

    sim_logger = logger;

    auto log_lvl = spdlog::level::info;
    switch(LOG_LEVEL) {
        case LOG_WARN : log_lvl = spdlog::level::warn; break;
        case LOG_INFO : log_lvl = spdlog::level::info; break;   
        case LOG_DEBUG: log_lvl = spdlog::level::debug; break;
        default:std::cout << "Invalid log level specified" << std::endl; std::exit(-1);   
    }

    sim_logger->set_level(log_lvl);
    sim_logger->flush_on(spdlog::level::err);

}

DLL_ATTRIB void init_debugger(std::shared_ptr<spdlog::logger>& logger);