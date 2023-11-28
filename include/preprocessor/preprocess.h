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

    enum endline_marker {
        ENDLINE_RN,
        ENDLINE_N,
        ENDLINE_R,
        ENDLINE_NONE
    };

    const std::vector<char>& contents;
    std::string output;
    bool handle_directives;
    bool handle_line_info;
    size_t buffer_index;
    size_t line_number;
    parser_state state;
    void handle_line_comment();
    void handle_block_comment();
    std::string process_token(std::string_view cur_token);
    std::string handle_token(std::string_view cur_token);

    bool no_advance;
    static sym_table table;
    bool is_end_of_line(bool do_skip = true);
    bool is_end_of_buf();
    bool is_alpha_num();
    bool is_valid_ident(std::string_view ident);
    endline_marker figure_new_line();
    std::string fetch_end_line_marker();
    void skip_newline(bool add_to_output = false);
public:
    static void init_with_defaults();
    preprocess(const std::vector<char>& input, bool handle_directives = false, 
    bool handle_line_info = false);
    void parse();
    std::string_view get_output() const;
};