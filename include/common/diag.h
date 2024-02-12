#pragma once

#include <string>
#include <vector>

class diag {
    std::string file_name;
    size_t start_line;   
    std::vector<std::string> lines;
    
    void split_into_lines(const std::vector<char>& file_content);
public:
    
    void print_error(size_t position);
    void init(std::string_view new_file_name, size_t start_line, const std::vector<char>& file_content);
};
