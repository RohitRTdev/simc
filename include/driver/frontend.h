#pragma once

#include <vector>
#include <string>

class frontend {
    std::string exe_path;
    std::vector<std::string> args;
    std::vector<std::string> output_files;
    std::vector<std::string> input_files;


public:
    frontend(const std::string& exe, const std::vector<std::string>& cmd_args, const std::vector<std::string>& i_files,
        const std::vector<std::string>& o_files);
    bool invoke() &&;
};
