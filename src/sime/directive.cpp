#include <unordered_set>
#include <filesystem>
#include "preprocessor/preprocess.h"
#include "preprocessor/parser.h"
#include "common/file-utils.h"
#include "debug-api.h"

void preprocess::handle_ifdef(std::string_view expression, std::string_view directive) {
    //This function could be called in both passive_scan mode or normal mode
    bool expr_res = false;
    auto exp = trim_whitespace(std::string(expression));

    //This is to help with the passive scan check only
    auto type = (directive == "elif" || directive == "else" || directive == "endif") ? IFDEF::ELIF : IFDEF::IF;
    if(directive == "elif" || directive == "else" || directive == "endif") {
       if(!ifdef_stack.size()) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("{} statement found in wrong context", directive);
       }
    }
    
    if(!ifdef_stack.size() || (type != IFDEF::IF && !ifdef_stack.top().state) || 
            (type == IFDEF::IF && !context.passive_scan)) {
        //This means passive_scan is not turned on within this parent's context
        //Hence, check for valid syntax
        if(directive == "if" || directive == "ifdef" || directive == "ifndef" || directive == "elif") {
            if(!exp.size()) {
                diag_inst.print_error(dir_line_start_idx);
                sim_log_error("{} statement require an expression", directive);
            }
            if(directive == "ifdef" || directive == "ifndef") {
                if(!is_valid_macro(exp)) {
                    diag_inst.print_error(dir_line_start_idx);
                    sim_log_error("{} statement requires a valid macro", directive);
                }
                auto [present, _] = table.has_symbol(exp);
                expr_res = directive == "ifdef" ? present : !present;
                sim_log_debug("Macro:{} is {}", exp, present ? "present" : "not present");
            }
            else {
                sim_log_debug("Starting token expansion phase on exp:{}", exp);
                std::vector<char> input(exp.begin(), exp.end());
                preprocess aux_preprocessor(input);
                aux_preprocessor.config_diag(this);
                aux_preprocessor.context.no_hash_processing = true;
                aux_preprocessor.context.process_defined_token = true;
                aux_preprocessor.parse(); 
                sim_log_debug("Token expanded output:{}", aux_preprocessor.get_output());
                exp.assign(aux_preprocessor.get_output().begin(), aux_preprocessor.get_output().end());

                sim_log_debug("Calling evaluator on exp:{}", exp);
                //Insert newline token to avoid lexer errors
                if(exp[exp.size()-1] != '\n' || exp[exp.size()-1] != '\r') {
                    exp.push_back('\n');
                }
                expr_res = evaluator(exp, &diag_inst, dir_line_start_idx);
            }
        }
        else {
            if(exp.size()) {
                diag_inst.print_error(dir_line_start_idx);
                sim_log_error("Invalid use of {} statement", directive);
            }
        }

    }
    if(directive == "if" || directive == "ifdef" || directive == "ifndef") {
        //Save the previous state, along with the eval result
        ifdef_stack.push(ifdef_info{IFDEF::IF, expr_res, context.passive_scan, dir_line_start_idx});
        
        //In passive scan mode, we only check for comments, line endings and #if/elif/else/endif blocks
        context.passive_scan = context.passive_scan || !expr_res;
        if(context.passive_scan) {
            sim_log_debug("Entering passive scan mode");
        }
    }
    else if(directive == "elif" || directive == "else") {
        if(ifdef_stack.top().type != IFDEF::IF && ifdef_stack.top().type != IFDEF::ELIF) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("{} statement found in wrong context", directive);
        }
        bool prev_state = ifdef_stack.top().state;
        bool prev_res = ifdef_stack.top().res;
        auto type = directive == "else" ? IFDEF::ELSE : IFDEF::ELIF;
        if(prev_res) {
            context.passive_scan = true;
        }
        else {
            if(directive == "else") {
                context.passive_scan = prev_state;
            }
            else {
                context.passive_scan = prev_state || !expr_res;
            }
            if(!context.passive_scan) {
                prev_res = true;
            }
        }
        ifdef_stack.pop();
        ifdef_stack.push(ifdef_info{type, prev_res, prev_state, dir_line_start_idx});
    }
    else if(directive == "endif") {
       //Go back to previous state
       context.passive_scan = ifdef_stack.top().state;
       if(!context.passive_scan && exp.size()) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("Invalid syntax for endif statement");
       }
        
       ifdef_stack.pop();
    }
}

std::tuple<bool, std::vector<std::string>, std::string_view, bool> 
preprocess::parse_macro_args(std::string_view macro_line) {
    if(!macro_line.size() || macro_line[0] != '(') {
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


    //Check for duplicates in arguments
    std::unordered_set<std::string> arg_table;
    for(const auto& arg: args) {
        if(arg_table.contains(arg)) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("Argument \"{}\" in macro definition seems to be duplicated", arg);
        }
        else {
            arg_table.insert(arg); 
        }
    }


    return make_tuple(true, args, macro_line.substr(idx), is_variadic);
}

void preprocess::handle_include(std::string_view dir_line) {
    
    size_t idx = 0;
    while(is_white_space(dir_line[idx]) && idx < dir_line.size())
        idx++;

    
    auto print_include_error = [&] {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Invalid syntax for include directive");
    };


    if (idx >= dir_line.size()) {
        print_include_error();
    }

    char delimiter = dir_line[idx];
    if(delimiter != '<' && delimiter != '\"') {
        print_include_error();    
    }

    if (delimiter == '<') {
        delimiter = '>';
    }

    std::string file_path;
    idx++;
    while(idx < dir_line.size() && dir_line[idx] != delimiter) {
        file_path.push_back(dir_line[idx++]);
    }

    if(idx >= dir_line.size() || dir_line[idx] != delimiter) {
        print_include_error();
    }

    file_path = trim_whitespace(file_path);
    sim_log_debug("Include file path read as:{}", file_path);

    //Start file read process
    std::string file_dir = std::filesystem::path(file_name).parent_path().string();
    sim_log_debug("Current file directory for {} is {}", file_name, file_dir);

    //Checking if file is present in current directory
    std::string path = (std::filesystem::path(file_dir) / std::filesystem::path(file_path)).string();
    sim_log_debug("Checking for file:{} in location:{}", file_path, path);

    std::optional<std::vector<char>> file_contents;

    file_contents = read_file(path, false);
    if(!file_contents.has_value()) {
        //Checking if file is present in current working directory
        sim_log_debug("Checking for file:{} in current working directory", file_path);
        file_contents = read_file(file_path, false);
        if(!file_contents.has_value()) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("File:{} not found", file_path);
        }
        path = file_path;
    }

    for(const auto& ancestor: ancestors) {
        if(std::filesystem::path(ancestor) == std::filesystem::path(path)) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("Cyclical inclusion of file:{} detected", path); 
        }
    }

    //Flush previous token if any
    place_barrier();
    sim_log_debug("Starting preprocessing for file:{}", file_path);
    preprocess aux_preprocessor(*file_contents);
    aux_preprocessor.init_diag(path);
    aux_preprocessor.ancestors.push_back(path);
    aux_preprocessor.parse();
    aux_preprocessor.ancestors.pop_back();
   

    output += aux_preprocessor.get_output();
    sim_log_debug("Preprocessed file:{}", file_path);
}

void preprocess::handle_undef(std::string_view dir_line) {
    auto [macro, next_idx] = read_next_token(dir_line);
    if(trim_whitespace(std::string(dir_line.substr(next_idx))).size()) {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Invalid syntax for undef statement");
    }

    if(!is_valid_macro(macro)) {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Invalid identifier name for macro:{}", macro);
    }

    if(table.has_symbol(macro).first) {
        sim_log_debug("Removing symbol:{}", macro);
        table.remove_symbol(macro);
    }
    else {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_warn("Symbol:{} doesn't seem to be defined. Ignoring undef stmt...", macro);
    }
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
    sim_log_debug("Starting line read for preprocessor directive");
    line_reader_inst.parse();

    sim_log_debug("Preprocess line read is:{}", line_reader_inst.get_output());
    dir_line_start_idx = buffer_index;
    auto [directive, next_idx] = read_next_token(line_reader_inst.get_output());

    if(context.passive_scan) {
        if(directive != "if" && directive != "ifdef" && directive != "ifndef" && directive != "elif" && directive != "else" && directive != "endif") {
            sim_log_debug("Ignoring directive as it is found during passive scan");
            return;
        }
    }

    if(directive == "include") {
        //Start a line read but consider angle brackets as string constants
        sim_log_debug("Starting include argument read...");
        preprocess line_reader_inst(rem_line, false, true);
        line_reader_inst.context.consider_angle_as_str = true;
        line_reader_inst.parse();
        buffer_index += line_reader_inst.buffer_index;
        line_number += line_reader_inst.line_number - 1;

        handle_include(line_reader_inst.get_output().substr(next_idx));
        return;
    }
    else {
        buffer_index += line_reader_inst.buffer_index;
        line_number += line_reader_inst.line_number - 1;
    }

    auto check_bounds = [&] {
        if(line_reader_inst.get_output().size() <= next_idx) {
            diag_inst.print_error(dir_line_start_idx);
            sim_log_error("Invalid syntax for {}", directive);
        }
    };
    
    if(directive == "define") {
        check_bounds();
        handle_define(line_reader_inst.get_output().substr(next_idx));
    }
    else if(directive == "undef") {
        check_bounds();
        handle_undef(line_reader_inst.get_output().substr(next_idx));
    }
    else if(directive == "if" || directive == "ifdef" ||  directive == "ifndef" || directive == "elif" || directive == "else" || directive == "endif") {
        if(directive == "if" || directive == "ifdef" || directive == "ifndef" || directive == "elif")
            check_bounds();
        
        handle_ifdef(line_reader_inst.get_output().substr(next_idx), directive);
    }
    else {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Unrecognized preprocessor directive:{}", directive);
    }


}

