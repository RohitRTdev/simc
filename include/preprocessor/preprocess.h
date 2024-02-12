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
    struct preprocess_context {
        bool handle_directives;
        bool read_single_line;
        bool read_macro_arg;
        bool in_macro_expansion;
        bool in_token_expansion;
        bool in_arg_prescan_mode;
        bool is_variadic_macro;
        bool arg_premature_term;
        bool args_complete;
        bool prev_token_macro; 
        bool no_complete_expansion;
        bool macro_arg_expanded_token;
        bool no_hash_processing;
        bool is_last_token_fn_macro;
		std::vector<std::string> set_expanded_args;
        std::vector<std::string> set_actual_args;
        std::vector<std::string> macro_args;
        std::string actual_arg;
    
        void copy_macro_params(const preprocess_context& new_context) {
            in_macro_expansion = new_context.in_macro_expansion;
            is_variadic_macro = new_context.is_variadic_macro;
            set_expanded_args = new_context.set_expanded_args;
            set_actual_args = new_context.set_actual_args;
        }
    } context;
    size_t buffer_index;
    size_t prev_idx;
    size_t line_number;
    size_t diag_line_offset;
    size_t dir_line_start_idx;
    std::string prev_token;
    size_t prev_token_pos;
    size_t string_op_pos;
    size_t bracket_count;
    parser_state state;
    static sym_table table;
    static std::vector<std::string> parents;
    
    void handle_line_comment();
    void handle_block_comment();
    bool handle_continued_line(bool only_check = false);
    void handle_directive();
    void handle_define(std::string_view dir_line);
	void setup_prev_token_macro(const std::string& new_token); 
    std::tuple<bool, std::vector<std::string>, std::string_view, bool> parse_macro_args(std::string_view macro_line);
    std::string process_token(std::string_view cur_token);
    std::string expand_token(std::string_view cur_token);
    std::string expand_variadic_args();
    std::string stringify_token(std::string_view token);
    std::string macro_arg_expand(std::string_view token);
    void print_error(size_t pos);
    bool is_word_parent(std::string_view word);
    bool is_function_macro(std::string_view token);
    bool is_macro_arg(std::string_view token);
    bool is_end_of_line(bool do_skip = true);
    bool is_end_of_buf();
    bool is_alpha_num(char ch);
    bool is_alpha_num();
    bool is_valid_macro(std::string_view ident);
    bool is_valid_ident(std::string_view ident);
    void trim_whitespace(std::string& token);
    endline_marker figure_new_line();
    std::string fetch_end_line_marker();
    std::pair<std::string, size_t> read_next_token(std::string_view line);
    bool is_white_space();
    bool is_white_space(char ch);
    void skip_newline(bool add_to_output = false);
    bool is_alpha_numeric_token(std::string_view token);
    std::pair<std::string, std::string> split_token(std::string_view token);
    void config_diag(const preprocess* inst);
public:
    static void init_with_defaults();
    preprocess(const std::vector<char>& input, bool handle_directives = true, 
    bool read_single_line = false, bool read_macro_arg = false);
    void parse();
    void init_diag(std::string_view name, size_t line_num = 1);
    std::string_view get_output() const;
};
