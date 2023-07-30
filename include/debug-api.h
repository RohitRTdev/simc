#pragma once

#include <iostream>
#include "spdlog/fmt/fmt.h"

#ifdef SIMDEBUG
    #include "spdlog/spdlog.h"
    extern std::shared_ptr<spdlog::logger> sim_logger;
#endif

#ifdef SIMDEBUG
    #define sim_log_info( ... ) sim_logger->info(__VA_ARGS__)
    #define sim_log_debug( ... ) sim_logger->debug(__VA_ARGS__)
    #define sim_log_warn( ... ) sim_logger->warn(__VA_ARGS__)
    #define sim_log_error( ... ) {sim_logger->error(__VA_ARGS__); std::exit(-1);} 
    #define CRITICAL_ASSERT( cond, msg, ... ) {if(!(cond)) {sim_logger->critical(msg __VA_OPT__(,) __VA_ARGS__); std::exit(-2);}}
#else
    #define sim_log_info( ... )
    #define sim_log_debug( ... )
    #define sim_log_warn( ... )
    #define sim_log_error( msg, ... ) {std::cout << "[ERROR]:" << fmt::format(msg __VA_OPT__(,) __VA_ARGS__) << std::endl; std::exit(-1);}
    #define CRITICAL_ASSERT( cond, msg, ... ) { if(!cond) {std::cout << "[CRITICAL_ERROR]:" << fmt::format(msg __VA_OPT__(,) __VA_ARGS__) << std::endl; std::exit(-2);}}
#endif

