#pragma once
#include <string>
#include <vector>

class argparser {
    std::vector<std::string> input_files;
    std::vector<std::string> output_files;
    int m_argc;
    char** m_argv;
    std::string m_extension;
    std::string generate_default_file_name(std::string_view input_file);
public:
    argparser(int argc, char** argv, std::string_view extension);
    void parse();
    const std::vector<std::string>& get_output_files() const;
    const std::vector<std::string>& get_input_files() const;
};