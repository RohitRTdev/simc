#include "compiler/diag.h"
#include "debug-api.h"

std::string diag::file_name;
std::vector<std::string> diag::lines;
size_t diag::start_line = 0;
void diag::init(std::string_view new_file_name, const std::vector<char>& file_content) {
    file_name = new_file_name;
    start_line = 0;
    lines.clear();
    split_into_lines(file_content);
}


void diag::split_into_lines(const std::vector<char>& file_content) {
    std::string line;
    for (char c : file_content) {
        line += c;
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        }
    }

    // Add the last line if not terminated by a newline
    if (!line.empty()) {
        lines.push_back(line);
    }
}

void diag::print_error(size_t position) {
    size_t cur_pos = 0;
    size_t line_num = 1;
    for (const auto& line : lines) {
        cur_pos += line.length(); 
        if (cur_pos >= position) {
            size_t offset = position + line.length() - cur_pos; 
            std::cout << fmt::format("In File:{}:Line:{}:", file_name, line_num) << std::endl;
            if(line.ends_with('\n'))
                std::cout << line;
            else 
                std::cout << line << std::endl;
            std::cout << std::string(offset, ' ') << "^" << std::endl; 
            return;
        }
        line_num++;
    }

    CRITICAL_ASSERT_NOW("position_index:{} is not within the file:{}", position, file_name);
}