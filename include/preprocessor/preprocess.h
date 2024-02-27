#pragma once

#include <string>
#include <vector>
#include <stack>
#include "preprocessor/sym_table.h"
#include "common/diag.h"

enum parser_state {
    PARSER_NORMAL,
    PARSER_STRING,
    PARSER_COMMENT,
    PARSER_TOKEN
};


//Preprocessor parsing and state handling construct
class preprocess {

    //We emit whatever endline we find (This means, a file could be mixed with CRLF/LF/CR ending. However, it's not recommended to mix different endlines)
    enum endline_marker {
        ENDLINE_RN,
        ENDLINE_N,
        ENDLINE_R,
        ENDLINE_NONE
    };
    
    enum IFDEF {
        IF,
        ELIF,
        ELSE
    };

    struct ifdef_info {
        IFDEF type;
        bool res;
        bool state;
        size_t offset;
    };

    const std::vector<char>& contents;
    std::string output;
    std::string file_name;
    diag diag_inst;  // Gives us file diagnostic information (Helpful while printing diagnostic messages)
    struct preprocess_context {
        bool handle_directives;     // Turned on when handling top level file
        bool read_single_line;      // Indicates that we would like to read one logical line and do no expansion
        bool read_macro_arg;        // Indicates that we would like to read one logical macro argument
        bool in_macro_expansion;    // Turned on during function macro expansion
        bool in_token_expansion;    // Turned on during object/function macro expansion
        bool in_arg_prescan_mode;   // Turned on during argument prescan phase on function macro expansion
        bool is_variadic_macro;     
        bool arg_premature_term;    // Tells if function macro is prematurely terminated or not 
        bool args_complete;         // Tells if function macro is properly invoked or not
        bool prev_token_macro;      
        bool no_complete_expansion; 
        bool macro_arg_expanded_token;
        bool no_hash_processing;    // Turned on when we don't want to treat string('#') and concat('##') operators
        bool is_last_token_fn_macro;
        bool consider_angle_as_str;
        bool process_defined_token; // Tells if preprocessor should process the "defined()" operator
        bool passive_scan;
        std::vector<std::string> set_actual_args;
        std::vector<std::string> macro_args;
    
        void copy_macro_params(const preprocess_context& new_context) {
            in_macro_expansion = new_context.in_macro_expansion;
            is_variadic_macro = new_context.is_variadic_macro;
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
    std::stack<ifdef_info> ifdef_stack; 
    static sym_table table;
    static std::vector<std::string> parents; //Construct to prevent infinite recursion during macro expansion
    static std::vector<std::string> ancestors; //Construct to prevent infinite recursion during file inclusion 
    
    void handle_line_comment();
    void handle_block_comment();
    bool handle_continued_line(bool only_check = false);
    void handle_operator_defined();
    void handle_directive();
    void handle_include(std::string_view dir_line); 
    void handle_define(std::string_view dir_line);
    void handle_undef(std::string_view dir_line);
    void handle_ifdef(std::string_view expression, std::string_view directive); 
	void setup_prev_token_macro(const std::string& new_token); 
    void place_barrier();
    std::tuple<bool, std::vector<std::string>, std::string_view, bool> parse_macro_args(std::string_view macro_line);
    std::string process_token(std::string_view cur_token);
    std::string expand_token(std::string_view cur_token);
    std::string expand_variadic_args();
    std::string stringify_token(std::string_view token);
    void insert_token_at_pos(size_t pos, std::string_view token); 
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
    std::string trim_whitespace(const std::string& token);
    endline_marker figure_new_line();
    std::string fetch_end_line_marker();
    std::pair<std::string, size_t> read_next_token(std::string_view line);
    bool is_white_space();
    bool is_white_space(char ch);
    void skip_newline(bool add_to_output = false);
    bool is_alpha_numeric_token(std::string_view token);
    void config_diag(const preprocess* inst);
public:
    static std::vector<std::string> search_directories; 
   
    static void init_with_defaults(const std::string& top_file_name);
    preprocess(const std::vector<char>& input, bool handle_directives = true, 
    bool read_single_line = false, bool read_macro_arg = false);
    void parse();
    void init_diag(std::string_view name, size_t line_num = 1);
    std::string_view get_output() const;
};
