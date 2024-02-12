#include "preprocessor/preprocess.h"
#include "debug-api.h"

std::tuple<bool, std::vector<std::string>, std::string_view, bool> 
preprocess::parse_macro_args(std::string_view macro_line) {
    if(macro_line[0] != '(') {
        return make_tuple(false, std::vector<std::string>(), std::string_view(), false);
    }

    enum macro_arg_states {
        MACRO_ARG_START,
        MACRO_ARG_STARTED,
        MACRO_ARG_WAITING_FOR_END,
        MACRO_ARG_VARIADIC,
        MACRO_ARG_VARIADIC_END, 
        MACRO_ARG_END
    } state = MACRO_ARG_START;

    bool is_variadic = false;
    int idx = 1, dot_count = 0;
    std::string arg;
    std::vector<std::string> args;
    auto process_macro_arg_end = [&] (char ch) {
        args.push_back(arg);
        arg.clear();
        state = ch == ',' ? MACRO_ARG_START : MACRO_ARG_END;
    };

    auto print_macro_error = [&] {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Macro arg must be a valid identifier");
    };

    while(state != MACRO_ARG_END) {
        char ch = macro_line[idx];

        if(state == MACRO_ARG_START) {
            if(is_alpha_num(ch)) {
                arg += ch;
                state = MACRO_ARG_STARTED;
            }
            else if(ch == '.') {
                dot_count = 1;
                state = MACRO_ARG_VARIADIC;
            }
            else if(ch == ',' || ch == ')') {
                process_macro_arg_end(ch);
                args.push_back(arg);
                if(ch == ')') {
                    state = MACRO_ARG_END;
                }
            }
            else if(!is_white_space(ch)) {
                print_macro_error();
            }
        }
        else if(state == MACRO_ARG_STARTED) {
            if(ch == ',' || ch == ')') {
                process_macro_arg_end(ch);
            }
            else if(is_alpha_num(ch)) {
                arg += ch;
            }
            else if(is_white_space(ch)) {
                args.push_back(arg);
                arg.clear();
                state = MACRO_ARG_WAITING_FOR_END;
            }
            else {
                print_macro_error(); 
            }
        }
        else if(state == MACRO_ARG_WAITING_FOR_END) {
            if(ch == ',' || ch == ')') {
                process_macro_arg_end(ch);
            }
            else if(!is_white_space(ch)) {
                print_macro_error();
            }
        }
        else if(state == MACRO_ARG_VARIADIC) {
            if(ch == '.') {
		        dot_count++;                
            }
	        else {
		        print_macro_error();
	        }

            if(dot_count == 3) {
                state = MACRO_ARG_VARIADIC_END;
            }
        }
        else if(state == MACRO_ARG_VARIADIC_END) {
            if(ch == ')') {
                is_variadic = true;
                state = MACRO_ARG_END;
            }
            else if(!is_white_space(ch)) {
                print_macro_error();
            }
        }

        idx++;
    }

    return make_tuple(true, args, macro_line.substr(idx), is_variadic);
}

void preprocess::handle_define(std::string_view dir_line) {
    auto [macro, next_idx] = read_next_token(dir_line);

    //We're only considering object macros currently
    if(!is_valid_macro(macro)) {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Invalid identifier name for macro:{}", macro);
    }

    if(table.has_symbol(macro).first) {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Macro: {} is being redefined", macro);
    }

    auto [is_macro, macro_arg_list, body, is_variadic] = parse_macro_args(dir_line.substr(next_idx));
    bool has_value = is_macro && body.size();
    std::string value = is_macro ? std::string(body) : std::string();
    if(!is_macro) {
        for(char ch: dir_line.substr(next_idx)) {
            if(!is_white_space(ch)) {
                has_value = true;
                value = dir_line.substr(next_idx);
                break;
            }
        }
    }
    table.add_symbol(macro, has_value, is_macro, is_variadic, value);
    if(is_macro) {
        table.add_macro_args(macro, macro_arg_list);
    }
}

void preprocess::handle_directive() {
    std::vector<char> rem_line(contents.begin() + buffer_index + 1, contents.end());
    preprocess line_reader_inst(rem_line, false, true);
    line_reader_inst.parse();

    sim_log_debug("Preprocess line read is:{}", line_reader_inst.get_output());
    dir_line_start_idx = buffer_index;
    buffer_index += line_reader_inst.buffer_index;
    line_number += line_reader_inst.line_number - 1;

    auto [directive, next_idx] = read_next_token(line_reader_inst.get_output());

    if(directive == "define") {
        handle_define(line_reader_inst.get_output().substr(next_idx));
    }
    else {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Unrecognized preprocessor directive:{}", directive);
    }


}

