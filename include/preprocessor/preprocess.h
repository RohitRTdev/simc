#pragma once

#include <string>
#include <vector>
#include "preprocessor/sym_table.h"
#include "common/diag.h"

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
    std::string file_name;
    diag diag_inst;
    bool handle_directives;
    bool read_single_line;
    size_t buffer_index;
    size_t line_number;
    size_t dir_line_start_idx;
    parser_state state;
    std::vector<std::string> parents;
    void handle_line_comment();
    void handle_block_comment();
    bool handle_continued_line();
    void handle_directive();
    void handle_define(std::string_view dir_line);
    std::string process_token(std::string_view cur_token);
    std::string expand_token(std::string_view cur_token);
    bool is_word_parent(std::string_view word);

    bool no_advance;
    static sym_table table;
    bool is_end_of_line(bool do_skip = true);
    bool is_end_of_buf();
    bool is_alpha_num(char ch);
    bool is_alpha_num();
    bool is_valid_ident(std::string_view ident);
    endline_marker figure_new_line();
    std::string fetch_end_line_marker();
    std::pair<std::string, size_t> read_next_word(std::string_view line);
    bool is_white_space();
    bool is_white_space(char ch);
    void skip_newline(bool add_to_output = false);
public:
    static void init_with_defaults();
    preprocess(const std::vector<char>& input, bool handle_directives = true, 
    bool read_single_line = false);
    void parse();
    void init_diag(std::string_view name);
    std::string_view get_output() const;
};