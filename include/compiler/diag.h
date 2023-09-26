#pragma once

#include <string>
#include <vector>

class diag {
    static std::string file_name;
    static size_t start_line;   
    static std::vector<std::string> lines;
    
    static void split_into_lines(const std::vector<char>& file_content);
public:
    
    static void print_error(size_t position);
    static void init(std::string_view new_file_name, const std::vector<char>& file_content);
};
