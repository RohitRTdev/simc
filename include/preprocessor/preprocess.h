#pragma once

#include <string>
#include <vector>
#include "preprocessor/sym_table.h"

enum parser_state {
    PARSER_NORMAL,
    PARSER_COMMENT,
    PARSER_STRING,
    PARSER_TOKEN
};

class preprocess {
    const std::vector<char>& contents;
    std::string output;
    bool handle_directives;
    bool handle_line_info;
    size_t buffer_index;
    size_t line_number;
    parser_state state;
    void handle_line_comment();
    void handle_block_comment();
    std::string handle_token(std::string_view cur_token);

    bool no_advance;
    static sym_table table;
    bool is_end_of_line();
    void skip_newline();
public:
    preprocess(const std::vector<char>& input, bool handle_directives = false, 
    bool handle_line_info = false);
    void parse();
    std::string_view get_output() const;
};