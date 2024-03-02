#include "preprocessor/preprocess.h"
#include "debug-api.h"

sym_table preprocess::table;
std::vector<std::string> preprocess::parents;
std::vector<std::string> preprocess::ancestors;
std::vector<std::string> preprocess::search_directories; 

void preprocess::init_with_defaults(const std::string& top_file_name) {
    //This makes sure that we do not allow the compilation file to include itself
    ancestors.clear();
    ancestors.push_back(top_file_name);
}

void preprocess::insert_token_at_pos(size_t pos, std::string_view token) {
    if (pos == output.size()) {
        output += token;
    }
    else {
        output.insert(pos, token);
    }
};

//Replace line comments with a single space
void preprocess::handle_line_comment() {
    while(!is_end_of_buf()) {
        if(handle_continued_line()) {
            sim_log_debug("Handling continued line while handling line comment at line:{}", line_number-1);
        }
        else if(is_end_of_line(false)) {
            break;
        }
        else {
            buffer_index++;
        }
    }

    if(!context.passive_scan)
        output += " ";
}

void preprocess::handle_block_comment() {    
    std::string comment = "/*";
    bool found_star = false;

    buffer_index++; //Skip '*'
    while(!is_end_of_buf()) {
        char ch = contents[buffer_index];
        if(found_star) {
            if(handle_continued_line()) {
                continue;
            }
            
            if(ch == '/') {
                comment = " ";
                break;
            }
            else {
                found_star = false;
            }
        }
        
        if(!found_star && ch == '*') {
            found_star = true;
        }

        if (is_end_of_line(false)) {
            comment += fetch_end_line_marker(); //We need to preserve the newline as block comment might be left unterminated
            skip_newline();
        }
        else {
            comment += ch;
            buffer_index++;
        }
        
    }

    if(!context.passive_scan)
        output += comment;
}

bool preprocess::handle_continued_line(bool only_check) {
    if(contents[buffer_index] == '\\') {
        buffer_index++;
        if(is_end_of_line(!only_check)) {
            if(only_check) {
                buffer_index--;
            }
            return true;
        }
        buffer_index--;
    }

    return false;
}

void preprocess::handle_operator_defined() {
    size_t idx = buffer_index;
    size_t debug_idx = buffer_index - 4;
    //Syntax := defined macro OR defined ( macro )
    
    enum define_states {
        EXPECTING_DEF_LB,
        EXPECTING_DEF_MACRO,
        EXPECTING_DEF_RB
    } state = EXPECTING_DEF_LB;

    sim_log_debug("Encountered defined operator");
    std::string macro;
    bool in_lb_context = false, parse_success = false;
    while(idx < contents.size()) {
        char ch = contents[idx];
        if(state == EXPECTING_DEF_LB) {
            if(is_alpha_num(ch)) {
                state = EXPECTING_DEF_MACRO;
                idx--;
            }
            else if(ch == '(') {
                state = EXPECTING_DEF_MACRO;
                in_lb_context = true;
            }
            else if(!is_white_space(ch)) {
                print_error(debug_idx);
                sim_log_error("Invalid syntax for defined operator");
            }
        }
        else if(state == EXPECTING_DEF_MACRO) {
            if(is_alpha_num(ch)) {
                macro += ch;
            }
            else if(in_lb_context && ch == ')') {
                idx++;
                parse_success = true;
                break;
            }
            else if(in_lb_context && is_white_space(ch)) {
                state = EXPECTING_DEF_RB;
            }
            else if(!in_lb_context) {
                parse_success = true;
                break;
            }
            else {
                print_error(debug_idx);
                sim_log_error("Invalid syntax for defined operator");
            }
        }
        else if(state == EXPECTING_DEF_RB) {
            if(ch == ')') {
                idx++;
                parse_success = true;
                break;
            }
            else if(!is_white_space(ch)) {
                print_error(debug_idx);
                sim_log_error("Invalid syntax for defined operator");
            }
        }
        idx++;
    }

    if(!parse_success && state != EXPECTING_DEF_MACRO) {
        print_error(debug_idx);
        sim_log_error("Invalid syntax for defined operator");
    }

    sim_log_debug("Macro for operator defined found as:{}", macro);
    output += table.has_symbol(macro).first ? "1" : "0";
    buffer_index = idx;
}

std::string preprocess::expand_variadic_args() {
    std::string final_token;
    CRITICAL_ASSERT(context.in_macro_expansion && context.in_macro_expansion
    , "expand_variadic_args() called outside macro_expansion context");

    size_t start_idx = context.macro_args.size();
    for(int i = start_idx; i < context.set_actual_args.size(); i++) {
        final_token += context.set_actual_args[i];
        if(i != context.set_actual_args.size()-1) {
            final_token += ", ";
        }
    }

    return final_token;
}

std::string preprocess::expand_token(std::string_view cur_token) {
    std::string new_token(cur_token);
    auto res = table.has_symbol(new_token);
    if(res.first) {
        return res.second ? table[new_token] : "";
    }
    return new_token;
}

bool preprocess::is_word_parent(std::string_view word) {
    for(const auto& parent: parents) {
        if(parent == word) {
            return true;
        }
    }

    return false;
}

void preprocess::setup_prev_token_macro(const std::string& new_token) {
	prev_token = new_token;
    prev_token_pos = output.size();
	sim_log_debug("Continuing with prev_token:{}", prev_token);
}

std::string preprocess::process_token(std::string_view cur_token) {
    std::string final_token(cur_token);
    if (is_valid_ident(cur_token) && !context.read_single_line) {
        if(is_word_parent(cur_token)) {
            sim_log_debug("{} already found in expansion. Stopping further processing to prevent self recursion", cur_token);
            return final_token;
        }
        if(cur_token == "__VA_ARGS__") {
            if(!context.in_macro_expansion || !context.is_variadic_macro || !context.in_arg_prescan_mode) {
                print_error(buffer_index-std::string("__VA_ARGS__").size());
                sim_log_warn("__VA_ARGS__ is meaningful only within function macro expansion");
            }
            else if(context.in_arg_prescan_mode && context.is_variadic_macro) {
                return expand_variadic_args();
            }
        }
        auto stream = expand_token(cur_token);
        if (stream != final_token) {
            sim_log_debug("Found expandable token:{}", final_token);
            sim_log_debug("Token expanded to:{}", stream);

            std::vector<char> expanded_stream(stream.begin(), stream.end());
            sim_log_debug("Starting new preprocessor instance");
            preprocess aux_preprocessor(expanded_stream, false);
            aux_preprocessor.config_diag(this);
            aux_preprocessor.context.in_token_expansion = true;
            aux_preprocessor.parents.push_back(final_token);
            aux_preprocessor.parse();
            aux_preprocessor.parents.pop_back();
            sim_log_debug("Exiting preprocessor instance");
            sim_log_debug("Final translated output:{}", aux_preprocessor.get_output());   
            final_token = aux_preprocessor.get_output();
			context.prev_token_macro = aux_preprocessor.context.is_last_token_fn_macro;
			prev_token = aux_preprocessor.prev_token;
		}
    }
    return final_token;
}

std::string preprocess::macro_arg_expand(std::string_view token) {
    std::string final_token(token);
    if(!context.in_macro_expansion) {
        return final_token;
    }

    if(context.is_variadic_macro && token == "__VA_ARGS__") {
        return expand_variadic_args();
    }

    for(size_t i = 0; i < context.macro_args.size(); i++) {
        if(token == context.macro_args[i]) {
			return context.set_actual_args[i];
        }
    }

    return final_token;
}

bool preprocess::is_macro_arg(std::string_view token) {
    if (!context.in_macro_expansion) {
        return false;
    }

    if(context.is_variadic_macro && token == "__VA_ARGS__") {
        return true;
    }

    for (const auto& arg : context.macro_args) {
        if (arg == token) {
            return true;
        }
    }

    return false;
}

bool preprocess::is_function_macro(std::string_view token) {
    return table.has_symbol(std::string(token)).first && table.is_macro(std::string(token)).first;
}

std::string preprocess::stringify_token(std::string_view token) {
    std::string res(token);

    for(size_t idx = 0; idx < res.size(); idx++) {
        if(res[idx] == '\\' || res[idx] == '\"') {
            res.insert(idx++, 1, '\\');
        }
    }
    res.insert(0, 1, '\"');
    res += '\"';

    return res;
}

void preprocess::place_barrier() {
    if(prev_token.size()) {
        sim_log_debug("Placing barrier. Flushing previous token:{}", prev_token);
        insert_token_at_pos(prev_token_pos, process_token(prev_token));
    }
    prev_token.clear();
    context.prev_token_macro = false;
}

preprocess::preprocess(const std::vector<char>& input, bool handle_directives, 
bool read_single_line, bool read_macro_arg) : contents(input), line_number(1), 
buffer_index(0), state(PARSER_NORMAL), bracket_count(1), prev_idx(1), prev_token_pos(0) {
    context.handle_directives = handle_directives;
    context.in_macro_expansion = false;
    context.is_variadic_macro = false;
    context.read_macro_arg = read_macro_arg;
    context.read_single_line = read_single_line;
    context.arg_premature_term = false;
    context.args_complete = false;
    context.prev_token_macro = false;
    context.no_complete_expansion = false;
    context.macro_arg_expanded_token = false;
    context.in_arg_prescan_mode = false;
    context.in_token_expansion = false;
    context.no_hash_processing = false;
    context.is_last_token_fn_macro = false;
    context.passive_scan = false;
    context.consider_angle_as_str = false;
    context.process_defined_token = false;
}

void preprocess::init_diag(std::string_view name, size_t line_num) {
    file_name = name;
    diag_line_offset = line_num;
    diag_inst.init(name, line_num, contents);
}

void preprocess::config_diag(const preprocess* inst) {
    init_diag(inst->file_name, inst->diag_line_offset+inst->line_number-1);
}

void preprocess::parse() {
    bool start_of_line = true;
    bool found_string_op = false, found_concat_op = false;
    size_t string_op_pos = 0, string_op_out_pos = 0;
    std::string cur_token;
    bool triggered_whitespace = false;
    bool no_advance = false;
    char delimiter = ' ';
    size_t delimiter_start_pos = 0;
    auto setup_prev_token = [&](std::string_view token) {
        prev_token = token;
        prev_token_pos = output.size();
    };

    auto flush_token = [&] (bool is_last_token = false) {
        if(prev_token.size()) {
            if(is_last_token && !context.passive_scan && !context.read_single_line && !context.read_macro_arg && !context.in_arg_prescan_mode && is_function_macro(prev_token)) {
                sim_log_debug("Last token:{} seems to be a function macro. Saving it...", prev_token);
				context.is_last_token_fn_macro = true;
                return;
            }
            else if(context.in_arg_prescan_mode) {
                sim_log_debug("Macro expanding token:{} in prescan mode", prev_token);
                auto new_token = macro_arg_expand(prev_token);
                insert_token_at_pos(prev_token_pos, new_token);
            }
            else if(context.passive_scan) {
                //We don't output the text
                sim_log_debug("Ignoring token:{} in passive scan mode", prev_token);
            }
            else if (!is_alpha_numeric_token(prev_token) || context.prev_token_macro 
            || is_function_macro(prev_token) || context.no_complete_expansion 
            || context.read_single_line || context.read_macro_arg) {
                sim_log_debug("Appending token:{}", prev_token);

				insert_token_at_pos(prev_token_pos, prev_token);
            }
            else {
                sim_log_debug("Processing token:{}", prev_token);
                insert_token_at_pos(prev_token_pos, process_token(prev_token));
                if (context.prev_token_macro) {
        			setup_prev_token_macro(prev_token);
                    context.prev_token_macro = false;
                    return;
                }
            }
            prev_token.clear();
            context.prev_token_macro = false;
        }
    };

    auto check_string_op = [&] () {
        if(found_string_op) {
            print_error(string_op_pos);
            sim_log_error("Stringify operator must be followed by macro parameter");
        }
        else if(found_concat_op) {
            print_error(string_op_pos);
            sim_log_error("Concatenation operator must be followed by a valid token");
        }
    };

    auto handle_token = [&] {
        sim_log_debug("Found token:{}", cur_token);
        if(found_concat_op) {
            auto first_token = trim_whitespace(macro_arg_expand(prev_token));
            auto second_token = trim_whitespace(macro_arg_expand(cur_token));
            first_token += second_token;
            found_concat_op = false;
            output.erase(prev_token_pos);

            sim_log_debug("Pasted token before expansion:{}", first_token);
            setup_prev_token(first_token);
            if(!is_alpha_numeric_token(prev_token)) {
                print_error(string_op_pos);
                sim_log_warn("Pasting non alpha numeric tokens together");
            }
            context.prev_token_macro = false;
        }
        else if(found_string_op) {
            if (!context.in_arg_prescan_mode) {
                //String operator has no meaning outside of a function macro
                insert_token_at_pos(string_op_out_pos, "#");
                flush_token();
                setup_prev_token(cur_token);
            }
            else {
                if (!is_alpha_numeric_token(cur_token)) {
                    print_error(string_op_pos);
                    sim_log_error("String operator must be followed by a valid preprocessor token");
                }
                else if (!is_macro_arg(cur_token)) {
                    print_error(string_op_pos);
                    sim_log_error("String operator must be followed by function macro argument");
                }

                //Remove any whitespace that was added inbetween op and token 
                output.erase(string_op_out_pos);

                auto new_token = macro_arg_expand(cur_token);
                insert_token_at_pos(string_op_out_pos, stringify_token(trim_whitespace(new_token)));
                flush_token();
            }
            
            found_string_op = false;
            context.prev_token_macro = false;
            triggered_whitespace = false;
        }
        else {
            flush_token();
            setup_prev_token(cur_token);
            context.prev_token_macro = false;
            
        }
        cur_token.clear();
        start_of_line = false;
        no_advance = true;
        state = PARSER_NORMAL;
    };


    auto print_unterm_str_warn = [&] {
        if(context.read_single_line || context.read_macro_arg || context.in_arg_prescan_mode) {
            return;
        }
        print_error(delimiter_start_pos);
        sim_log_warn("Unterminated '{}'", delimiter == '>' ? '<' : delimiter);
    };
    
    while (buffer_index < contents.size()) {
        char ch = contents[buffer_index];
        if(state == PARSER_NORMAL) {
            if(!context.passive_scan && !context.read_single_line && !context.read_macro_arg && !context.in_arg_prescan_mode && ch == '(') {
				if(!is_function_macro(prev_token)) {
					flush_token(); //Forcefully expand the previous token
				}

				if(prev_token.size() && is_function_macro(prev_token) && !context.no_complete_expansion && !is_word_parent(prev_token)) {
                    sim_log_debug("Starting argument read for macro:{}", prev_token);
                
                    std::vector<std::string> args;
                    bool macro_incomplete = false;
                    size_t offset = buffer_index + 1;
                    size_t idx = 0;

					while (1) {
                        std::vector<char> stream(contents.begin() + offset, contents.end());
                        preprocess aux_preprocessor(stream, false, false, true);
                        aux_preprocessor.config_diag(this);
                        aux_preprocessor.context.copy_macro_params(context);
                        aux_preprocessor.context.in_token_expansion = true;
                        aux_preprocessor.context.read_macro_arg = true;
                        aux_preprocessor.parse();
                        sim_log_debug("Argument at index {} read as:{}", idx + 1, aux_preprocessor.get_output());

                        if(aux_preprocessor.context.arg_premature_term) {
                            //Flush all args we've got so far
                            macro_incomplete = true;
                            break;
                        }
                        else if(aux_preprocessor.context.args_complete) {
                            args.push_back(std::string(aux_preprocessor.get_output()));
                            offset += aux_preprocessor.buffer_index + 1;
                            line_number += aux_preprocessor.line_number - 1;
                            break;
                        }

                        offset += aux_preprocessor.buffer_index + 1;
                        line_number += aux_preprocessor.line_number - 1;
                        args.push_back(std::string(aux_preprocessor.get_output()));    
                        idx++;
                    } 

                    const auto& macro = table.fetch_macro_args(prev_token);
                    bool is_var = table.is_macro(prev_token).second;
                    if(macro_incomplete) {
                        print_error(prev_token_pos);
                        sim_log_error("Incorrect invocation of function macro:{}", prev_token);
                    }
                    else if((is_var && args.size() < macro.size()-1) || (!is_var && args.size() != macro.size() - 1)) {
                        print_error(buffer_index);
                        sim_log_error("Number of macro arguments given ({}) is different from number of argument in macro definition ({})", args.size(), macro.size());
                    }
                    else {
                       	std::vector<int> offsets; 
                        std::vector<char> input(macro[0].begin(), macro[0].end());
                        {
                            sim_log_debug("Starting function macro prescan for macro:{}", prev_token);
                            preprocess aux_preprocessor(input, false);
                            aux_preprocessor.config_diag(this);
                            aux_preprocessor.context.in_macro_expansion = true;
                            aux_preprocessor.context.in_token_expansion = true;
                            aux_preprocessor.context.is_variadic_macro = is_var;
                            aux_preprocessor.context.set_actual_args = args;
                            aux_preprocessor.context.macro_args.assign(macro.begin()+1, macro.end());
                            aux_preprocessor.context.in_arg_prescan_mode = true;
                            aux_preprocessor.parse();
                            sim_log_debug("Prescan o/p for macro:{} is:{}", prev_token, aux_preprocessor.get_output());
                            input.assign(aux_preprocessor.get_output().begin(), aux_preprocessor.get_output().end());
						}
                        
                        //Start function macro expansion
                        sim_log_debug("Starting function macro expansion for macro:{}", prev_token);
                        preprocess aux_preprocessor(input, false);
                        aux_preprocessor.config_diag(this);
                        aux_preprocessor.context.in_macro_expansion = true;
                        aux_preprocessor.context.in_token_expansion = true; 
                        aux_preprocessor.context.is_variadic_macro = is_var;
                        aux_preprocessor.context.set_actual_args = args;
                        aux_preprocessor.context.no_hash_processing = true;
                        aux_preprocessor.context.macro_args.assign(macro.begin()+1, macro.end());

                        //To prevent self recursion
                        parents.push_back(prev_token);
                        aux_preprocessor.parse();
                        parents.pop_back();
                        sim_log_debug("Macro expansion for macro:{} is:{}", prev_token, aux_preprocessor.get_output());
        					
						insert_token_at_pos(prev_token_pos, aux_preprocessor.get_output());
						if(aux_preprocessor.context.is_last_token_fn_macro) {
							setup_prev_token_macro(aux_preprocessor.prev_token);
							context.prev_token_macro = false;
						}
						else {
							prev_token.clear();
						}
                        buffer_index = offset;
                        no_advance = true;
                    }
                }
                else {
                    flush_token();
                    setup_prev_token("(");
                }
            }
            else if(!context.passive_scan && !context.no_hash_processing && !context.read_macro_arg && context.in_token_expansion && !context.read_single_line && ch == '#') {
                if (found_concat_op) {
                    print_error(string_op_pos);
                    sim_log_error("Concatenation operator must be followed by a valid token");
                }
                else if (found_string_op && triggered_whitespace) {
                    if (context.in_macro_expansion) {
                        print_error(string_op_pos);
                        sim_log_error("String operator requires macro argument as parameter");
                    }
                    sim_log_debug("Found another potential string operator while having a string operator. Flushing previous operator");
                    insert_token_at_pos(string_op_out_pos, "#");
                    found_string_op = false;
                    triggered_whitespace = false;
                }

                if(found_string_op && !triggered_whitespace) {
                    sim_log_debug("Found concatenation operator");
                    found_string_op = false;
                    found_concat_op = true;
                    if(!prev_token.size()) {
                        print_error(string_op_pos);
                        sim_log_error("Concatenation operator must be preceded by a valid token");
                    }

                    if (context.prev_token_macro) {
                        print_error(string_op_pos);
                        sim_log_error("Cannot paste ')' with another token");
                    }
                }
                else if(!found_string_op) {
                    sim_log_debug("Found string operator '#'");
                    found_string_op = true;
                    triggered_whitespace = false;
                    string_op_pos = buffer_index;
                    string_op_out_pos = output.size();
                }
            }
            else if(handle_continued_line()) {
                sim_log_debug("Handling continued line in NORMAL state at line:{}", line_number-1);
                no_advance = false;
                continue;
            }
            else if(ch == '/') {
                state = PARSER_COMMENT;
            }
            else if(ch == '\"' || ch == '\'' || (context.consider_angle_as_str && ch == '<')) {
                check_string_op();     
                sim_log_debug("Handling string");
                state = PARSER_STRING;
                delimiter = ch == '<' ? '>' : ch;
                if(!context.passive_scan)
                    output += ch;
                start_of_line = false;
                flush_token();
                delimiter_start_pos = buffer_index;
            }
            else if(context.read_macro_arg && (ch == ',' || ch == '(' || ch == ')')) {
				bool added_to_output = false;
				if(ch == ',') {
                    if(bracket_count == 1) {
                        break;
                    }
                }
                else if(ch == '(') {
                    bracket_count++;
                }
                else if(ch == ')') {
                    if(bracket_count == 1) {
                        context.args_complete = true;
                        break;
                    }
                    else {
                        bracket_count--;
            			output += ch;
						added_to_output = true;
					}
                }

				if(bracket_count != 1 && !added_to_output) {
					output += ch;
				}
            }
            else if((context.handle_directives || context.passive_scan) && ch == '#' && start_of_line) {
                sim_log_debug("Preprocessor directive detected at line:{}", line_number);
                handle_directive();
            }
            else {
                if(is_end_of_line(false)) {
                    check_string_op();
                    start_of_line = true;
                    no_advance = true;
                    if(context.read_single_line) {
                        sim_log_debug("EOL detected in read_single_line mode");
                        break;
                    }
                    else if(context.read_macro_arg) {
                        sim_log_debug("EOL detected in read_macro_arg mode");
                        if(!output.size() || (output[output.size()-1] != ' ' 
                        && output[output.size()-1] != '\t')) {
                            output += ' ';
                        }
                        skip_newline(); 
                    } 
                    else {
                        skip_newline(!context.passive_scan);
                    }
                    flush_token();
                    sim_log_debug("Processing line number:{}", line_number);
                }
                else if(is_alpha_num()) {
                    state = PARSER_TOKEN;
                    no_advance = true;
                }
                else {
                    if(!is_white_space()) {
                        cur_token = ch;
                        handle_token();
                        start_of_line = false;
                        no_advance = false;
                    }
                    else {
                        if(!context.passive_scan)
                            output += ch;
                        if(found_string_op) {
                            triggered_whitespace = true;
                        }
                    }
                }
            }
        }
        else if(state == PARSER_COMMENT) {
            if(handle_continued_line()) {
                sim_log_debug("Handling continued line while handling possible comment at line:{}", line_number-1);
                no_advance = false;
                continue;
            }
            else if(ch == '/') {
                sim_log_debug("Handling line comment");
                handle_line_comment();
                no_advance = true;
            }
            else if(ch == '*') {
                sim_log_debug("Handling block comment");
                handle_block_comment();
            }
            else {
                start_of_line = false;
                flush_token();
                setup_prev_token("/");
                no_advance = true;
            }
            state = PARSER_NORMAL;
        }
        else if(state == PARSER_STRING) {
            if(handle_continued_line()) {
                sim_log_debug("Handling continued line while handling string at line:{}", line_number-1);
                no_advance = false;
                continue;
            }
            else if(ch == delimiter) {
                state = PARSER_NORMAL;
                if(!context.passive_scan)
                    output += ch;
            }
            else if(is_end_of_line(false)) {
                state = PARSER_NORMAL;
                print_unterm_str_warn();
                no_advance = true;
            }
            else {
                if(!context.passive_scan)
                    output += ch;
            }
          
        }
        else if(state == PARSER_TOKEN) {
            if(is_alpha_num()) {
                cur_token += ch;
            }
            else if(handle_continued_line()) {
                sim_log_debug("Handling continued line while handling token at line:{}", line_number - 1);
                no_advance = false;
                continue;
            }
            else {
                if(context.process_defined_token && cur_token == "defined") {
                    flush_token();
                    handle_operator_defined();
                    cur_token.clear();
                    state = PARSER_NORMAL;
                    no_advance = true;
                }
                else {
                    handle_token();
                }
            }
        }

        if(!no_advance)
            buffer_index++;
        no_advance = false;
    }
    if(state == PARSER_STRING) {
        print_unterm_str_warn();
    }
    
    if (context.read_macro_arg && buffer_index >= contents.size() && !context.args_complete) {
        context.arg_premature_term = true;
    }

    if(state == PARSER_COMMENT) {
        if(!context.passive_scan)
            output += '/';
    }

    if(context.handle_directives && ifdef_stack.size()) {
        diag_inst.print_error(ifdef_stack.top().offset);
        sim_log_error("Directive was not terminated"); 
    }

    if(prev_token.size() && cur_token.size()) {
        if(found_string_op || found_concat_op) {
            handle_token();
            //Once concatenation is done, new token could be function macro 
            flush_token(true);
        }
        else {
            flush_token(); //Flush the previous token
            setup_prev_token(cur_token);    
            flush_token(true); //Flush current token but check if it's function macro
        }
    }
    else if(prev_token.size()) {
        check_string_op();
		flush_token(true);
    }
    else if(cur_token.size()) {
        if(found_concat_op) {
            print_error(string_op_pos);
            sim_log_error("Concatenation operator should not show up at the beginning of a macro");
        }
		else if(found_string_op) {
			handle_token();
		}
		else {
			setup_prev_token(cur_token);
			flush_token(true);
		}
	}
}

void preprocess::print_error(size_t pos) {
    if(context.in_token_expansion) {
        //We're currently expanding a token
        std::cout << "In expansion of token:" << parents[parents.size()-1] << std::endl;
    }
    diag_inst.print_error(pos);
}

std::string_view preprocess::get_output() const {
    return output;
}
