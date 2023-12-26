#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class argparser {
    std::vector<std::string> input_files;
    std::vector<std::string> output_files;
    int m_argc;
    char** m_argv;
    std::string m_extension;
    std::unordered_map<char, bool> flag_store;
    std::unordered_map<char, std::vector<std::string>> name_flag_store;
    std::string generate_default_file_name(std::string_view input_file);
public:

    enum flag_type {
        NORMAL,
        FILE
    };

    argparser(int argc, char** argv, std::string_view extension);
    void add_flag(char ch, flag_type type);
    void parse();
    const std::vector<std::string>& get_output_files() const;
    const std::vector<std::string>& get_input_files() const;
};