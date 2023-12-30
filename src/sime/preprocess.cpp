#include "preprocessor/preprocess.h"
#include "debug-api.h"

sym_table preprocess::table;
std::vector<std::string> preprocess::parents;

void preprocess::init_with_defaults() {
}

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

    output += " ";
    no_advance = true;
}

void preprocess::handle_block_comment() {    
    std::string comment = "/*";
    bool found_star = false;

    buffer_index++; //Skip '*'
    while(!is_end_of_buf()) {
        char ch = contents[buffer_index];
        if(found_star) {
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

    output += comment;
}

bool preprocess::handle_continued_line() {
    if(contents[buffer_index] == '\\') {
        buffer_index++;
        if(is_end_of_line()) {
            return true;
        }
        buffer_index--;
    }

    return false;
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

std::string preprocess::process_token(std::string_view cur_token) {
    std::string final_token(cur_token);
    if (is_valid_ident(cur_token) && !read_single_line) {
        if(is_word_parent(cur_token)) {
            sim_log_debug("{} already found in expansion. Stopping further processing to prevent self recursion", cur_token);
            return final_token;
        }
        auto stream = expand_token(cur_token);
        if (stream != final_token) {
            sim_log_debug("Found expandable token:{}", final_token);
            sim_log_debug("Token expanded to:{}", stream);

            std::vector<char> expanded_stream(stream.begin(), stream.end());
            sim_log_debug("Starting new preprocessor instance");
            preprocess aux_preprocessor(expanded_stream, false, false);
            aux_preprocessor.init_diag(file_name, diag_line_offset + line_number - 1);
            aux_preprocessor.parents.push_back(final_token);
            aux_preprocessor.parse();
            sim_log_debug("Exiting preprocessor instance");
            sim_log_debug("Final translated output:{}", aux_preprocessor.get_output());   
            final_token = aux_preprocessor.get_output();
        }
    }
    return final_token;
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

preprocess::preprocess(const std::vector<char>& input, bool handle_directives, 
bool read_single_line) : contents(input), handle_directives(handle_directives),
line_number(1), buffer_index(0), state(PARSER_NORMAL),
no_advance(false), read_single_line(read_single_line)
{}

void preprocess::init_diag(std::string_view name, size_t line_num) {
    file_name = name;
    diag_line_offset = line_num;
    diag_inst.init(name, line_num, contents);
}

void preprocess::parse() {
    bool start_of_line = true;
    size_t prev_token_pos = 0;
    bool found_string_op = false, found_concat_op = false;
    size_t string_op_pos = 0, string_op_out_pos = 0;
    std::string cur_token;
    std::string prev_token;
    bool triggered_whitespace = false;
    auto flush_token = [&] {
        if(prev_token.size()) {
            sim_log_debug("Processing token:{}", prev_token);
            if(prev_token_pos == output.size()) {
                output += process_token(prev_token);
            }
            else {
                output.insert(prev_token_pos, process_token(prev_token));
            }
            prev_token.clear();
        }
    };

    auto check_string_op = [&] {
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
            prev_token += cur_token;
            found_concat_op = false;
            output.erase(prev_token_pos);
        }
        else if(found_string_op) {
            //Remove any whitespace that was added inbetween op and token 
            output.erase(string_op_out_pos); 
            
            size_t shift = output.size();
            flush_token();
            shift = output.size() - shift;
            output.insert(string_op_out_pos + shift, stringify_token(cur_token));
            
            found_string_op = false;
            triggered_whitespace = false;
        }
        else {
            flush_token();
            prev_token = cur_token;
            prev_token_pos = output.size();
        }
        cur_token.clear();
        start_of_line = false;
        no_advance = true;
        state = PARSER_NORMAL;
    };

    while (buffer_index < contents.size()) {
        char ch = contents[buffer_index];
        
        if(state == PARSER_NORMAL) {
            if(!read_single_line && !handle_directives && ch == '#') {
                if(found_string_op && !triggered_whitespace) {
                    sim_log_debug("Found concatenation operator");
                    found_string_op = false;
                    found_concat_op = true;
                    if(!prev_token.size()) {
                        print_error(string_op_pos);
                        sim_log_error("Concatenation operator must be preceded by a valid token");
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
            else if(ch == '\"') {
                check_string_op();     
                sim_log_debug("Handling string");
                state = PARSER_STRING;
                output += ch;
                start_of_line = false;
                flush_token();
            }
            else if(ch == '#' && start_of_line) {
                sim_log_debug("Preprocessor directive detected at line:{}", line_number);
                handle_directive();
            }
            else {
                if(is_end_of_line(false)) {
                    check_string_op();
                    start_of_line = true;
                    no_advance = true;
                    if(read_single_line) {
                        sim_log_debug("EOL detected in read_single_line mode");
                        break;
                    } else {
                        skip_newline(true);
                    }
                    flush_token();
                    sim_log_debug("Processing line number:{}", line_number);
                }
                else if(is_alpha_num()) {
                    state = PARSER_TOKEN;
                    no_advance = true;
                }
                else {
                    output += ch;
                    if(!is_white_space()) {
                        check_string_op();
                        start_of_line = false;
                        flush_token();
                    }
                    else {
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
            }
            else if(ch == '*') {
                sim_log_debug("Handling block comment");
                handle_block_comment();
            }
            else {
                check_string_op();
                start_of_line = false;
                output += "/";
                no_advance = true;
                flush_token();
            }
            state = PARSER_NORMAL;
        }
        else if(state == PARSER_STRING) {
            if(handle_continued_line()) {
                sim_log_debug("Handling continued line while handling string at line:{}", line_number-1);
                no_advance = false;
                continue;
            }
            else if(ch == '\"' || is_end_of_line(false)) {
                state = PARSER_NORMAL;
            }

            output += ch;
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
                handle_token();
            }
        }

        if(!no_advance)
            buffer_index++;
        no_advance = false;
    }
    
    if(state == PARSER_COMMENT) {
        output += "/";
    }

    if(prev_token.size() && !cur_token.size()) {
        check_string_op();
        flush_token();
    }
    else if(prev_token.size() || cur_token.size()) {
        handle_token();
        if(prev_token.size()) {
            flush_token();
        }
    }

}

void preprocess::print_error(size_t pos) {
    if(!handle_directives) {
        //We're currently expanding a token
        CRITICAL_ASSERT(parents.size(), "parents stack is empty within token expansion instance");
        std::cout << "In expansion of token:" << parents[parents.size()-1] << std::endl;
    }
    diag_inst.print_error(pos);
}

std::string_view preprocess::get_output() const {
    return output;
}
