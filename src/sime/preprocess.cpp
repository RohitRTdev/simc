#include "preprocessor/preprocess.h"
#include "debug-api.h"

sym_table preprocess::table;

void preprocess::init_with_defaults() {
    table.add_symbol("ANOTHER", true, "Rohit/*Comment is here */Jacob");
    table.add_symbol("REPLACE_THIS", true, "ANOTHER");
}


preprocess::endline_marker preprocess::figure_new_line() {
    auto index = buffer_index;
    bool has_end_r = false, has_end_n = false;
    if(index < contents.size()) {
        if(contents[index] == '\r') {
            has_end_r = true;
            index++;
        }
    }
    
    if(index < contents.size()) {
        if(contents[index] == '\n') {
            has_end_n = true;
            index++;
        }
    } 

    if(has_end_n && has_end_r) {
        return ENDLINE_RN;
    }
    else if(has_end_n) {
        return ENDLINE_N;
    }
    else if(has_end_r) {
        return ENDLINE_R;
    }
    
    return ENDLINE_NONE;
}

std::string preprocess::fetch_end_line_marker() {
    auto end_line_type = figure_new_line();
    switch(end_line_type) {
        case ENDLINE_R: return "\r";
        case ENDLINE_N: return "\n";
        case ENDLINE_RN: return "\r\n";
        default: return "";
    }
}

void preprocess::skip_newline(bool add_to_output) {
    auto newline_type = figure_new_line();
    std::string ch = "";
    switch(newline_type) {
        case ENDLINE_R: {
            ch = "\r";
            buffer_index++;
            break;
        }
        case ENDLINE_N: {
            ch = "\n";
            buffer_index++;
            break;
        }
        case ENDLINE_RN:{
            buffer_index += 2;
            ch = "\r\n";
            break;
        }
    }

    if(add_to_output) {
        if(ch != "") {
            output += ch;
        }
    }
}

bool preprocess::is_end_of_buf() {
    return !(buffer_index < contents.size());
}

bool preprocess::is_alpha_num() {
    return isalnum(contents[buffer_index]) || contents[buffer_index] == '_';
}

bool preprocess::is_valid_ident(std::string_view ident) {
    return isalpha(ident[0]) || ident[0] == '_';
}

bool preprocess::is_end_of_line(bool do_skip) {
    //One of '\n' or '\r\n' or '\r'
    bool is_end_line_1 = contents[buffer_index] == '\n' || (contents.size() - buffer_index > 1 && contents[buffer_index] == '\r' && contents[buffer_index + 1] == '\n');
    bool is_end_line_2 = contents[buffer_index] == '\r';

    if(is_end_line_1 || is_end_line_2) {
        if(do_skip)
            skip_newline();
        return true;
    }

    return false; 
}

//Replace line comments with a single space
void preprocess::handle_line_comment() {
    while(!is_end_of_buf() && !is_end_of_line())
        buffer_index++;

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
            line_number++;
        }
        else {
            comment += ch;
            buffer_index++;
        }
        
    }

    output += comment;
}

std::string preprocess::handle_token(std::string_view cur_token) {
    std::string new_token(cur_token);
    if(table.has_symbol(new_token)) {
        return table[new_token];
    }

    return new_token;
}

std::string preprocess::process_token(std::string_view cur_token) {
    std::string final_token(cur_token);
    if (is_valid_ident(cur_token)) {
        auto stream = handle_token(cur_token);
        sim_log_debug("Found token:{}", final_token);
        if (stream != final_token) {
            sim_log_debug("Token translated to:{}", stream);
            std::vector<char> new_token(stream.begin(), stream.end());
            preprocess aux_preprocessor(new_token);
            aux_preprocessor.parse();
            final_token = aux_preprocessor.get_output();
        }
    }

    return final_token;
}

preprocess::preprocess(const std::vector<char>& input, bool handle_directives, 
bool handle_line_info) : contents(input), handle_directives(handle_directives),
handle_line_info(handle_line_info), line_number(1), buffer_index(0), state(PARSER_NORMAL),
no_advance(false)
{}

void preprocess::parse() {
    bool start_of_line = true;
    bool found_escaped_character = false;
    std::string cur_token;
    while (buffer_index < contents.size()) {
        char ch = contents[buffer_index];
        
        if(found_escaped_character) {
            if(!is_end_of_line()) {
                output += '\\';
                output += ch;
            }
            else {
                line_number++;
            }
            found_escaped_character = false;
        }

        if(ch == '\\') {
            found_escaped_character = true;
            buffer_index++;
            continue;
        }

        if(state == PARSER_NORMAL) {
            if(ch == '/') {
                state = PARSER_COMMENT;
            }
            else if(ch == '\"') {
                sim_log_debug("Handling string");
                state = PARSER_STRING;
                output += ch;
            }
            else if(ch == '#' && start_of_line && handle_directives) {
                sim_log_debug("Start handling directive");
                //handle_preprocessor_directive(contents);
            }
            else {
                if(is_end_of_line(false)) {
                    skip_newline(true);
                    start_of_line = true;
                    line_number++;
                    no_advance = true;
                    sim_log_debug("Processing line number:{}", line_number);
                }
                else if(is_alpha_num()) {
                    start_of_line = false;
                    cur_token.clear();
                    state = PARSER_TOKEN;
                    no_advance = true;
                }
                else {
                    output += ch;
                }
            }
        }
        else if(state == PARSER_COMMENT) {
            if(ch == '\n') {
                state = PARSER_NORMAL;
            }
            else if(ch == '\r') {
                state = PARSER_COMMENT;
                continue;
            }
            else if(ch == '/') {
                sim_log_debug("Handling line comment");
                handle_line_comment();
                start_of_line = true;
                line_number++;
            }
            else if(ch == '*') {
                sim_log_debug("Handling block comment");
                handle_block_comment();
            }
            else {
                start_of_line = false;
                (output += "/") += ch;
            }
            state = PARSER_NORMAL;
        }
        else if(state == PARSER_STRING) {
            if(ch == '\"' || is_end_of_line(false)) {
                state = PARSER_NORMAL;
            }
            output += ch;
        }
        else if(state == PARSER_TOKEN) {
            if(is_alpha_num()) {
                cur_token += ch;
            }
            else {
                output += process_token(cur_token);
                start_of_line = false;
                no_advance = true;
                state = PARSER_NORMAL;
            }
        }

        if(!no_advance)
            buffer_index++;
        no_advance = false;
    }

    if(state == PARSER_COMMENT) {
        output += "/";
    }

    if (state == PARSER_TOKEN) {
        output += process_token(cur_token);
    }
}

std::string_view preprocess::get_output() const {
    return output;
}
