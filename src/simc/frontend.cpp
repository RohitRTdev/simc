#include <cstdlib>
#include "driver/frontend.h"
#include "debug-api.h"

frontend::frontend(const std::string& exe, const std::vector<std::string>& cmd_args, const std::vector<std::string>& i_files, 
        const std::vector<std::string>& o_files): exe_path(exe), args(cmd_args), input_files(i_files), output_files(o_files) {
}


bool frontend::invoke() && {
    std::string cmdline_str;
    cmdline_str = exe_path + ' ';
    for(const auto& arg: args) {
        cmdline_str += arg + ' ';
    }

    for(const auto& o_file: output_files) {
        cmdline_str += " -o " + o_file;
    }

    for(const auto& i_file: input_files) {
        cmdline_str += " " + i_file;
    }

    sim_log_debug("Final cmdline string:{}", cmdline_str);
    
    int exit_code = std::system(cmdline_str.c_str());
    return exit_code == 0;
}


