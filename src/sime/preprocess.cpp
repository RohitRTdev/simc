#include "preprocessor/preprocess.h"
#include "debug-api.h"

sym_table preprocess::table;

void preprocess::skip_newline() {
    if(buffer_index < contents.size()) {
        if(contents[buffer_index] == '\r')
            buffer_index++;
    }
    
    if(buffer_index < contents.size()) {
        if(contents[buffer_index] == '\n')
            buffer_index++;
    }
}

bool preprocess::is_end_of_line() {
    //One of '\n' or '\r\n' or '\r'
    bool is_end_line_1 = contents[buffer_index] == '\n' || (contents.size() - buffer_index > 1 && contents[buffer_index] == '\r' && contents[buffer_index + 1] == '\n');
    bool is_end_line_2 = contents[buffer_index] == '\r';

    return is_end_line_1 || is_end_line_2;
}

//Replace line comments with a single space
void preprocess::handle_line_comment() {
    while(buffer_index < contents.size() &&
    !is_end_of_line())
    buffer_index++;

    output += " ";
    skip_newline();
    no_advance = true;
}

void preprocess::handle_block_comment() {    
    std::string comment;
    bool found_star = false;

    while(buffer_index < contents.size()) {
        char ch = contents[buffer_index];
        comment += ch;
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
        buffer_index++;
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

preprocess::preprocess(const std::vector<char>& input, bool handle_directives, 
bool handle_line_info) : contents(input), handle_directives(handle_directives),
handle_line_info(handle_line_info), line_number(0), buffer_index(0), state(PARSER_NORMAL),
no_advance(false)
{}

void preprocess::parse() {
    bool start_of_line = true;
    std::string cur_token;
    while (buffer_index < contents.size()) {
        char ch = contents[buffer_index];
        if(state == PARSER_NORMAL) {
            if(ch == '/') {
                state = PARSER_COMMENT;
            }
            else if(ch == '\"') {
                state = PARSER_STRING;
                output += ch;
            }
            else if(ch == '#' && start_of_line && handle_directives) {
                sim_log_debug("Start handling directive");
                //handle_preprocessor_directive(contents);
            }
            else {
                if(is_end_of_line()) {
                    start_of_line = true;
                    line_number++;
                    sim_log_debug("Processing line number:{}", line_number);
                }
                else if(isalnum(ch)) {
                    start_of_line = false;
                    cur_token.clear();
                    state = PARSER_TOKEN;
                    no_advance = true;
                }
                if(!isalnum(ch))
                    output += ch;
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
            if(ch == '\"') {
                state = PARSER_NORMAL;
            }
            output += ch;
        }
        else if(state == PARSER_TOKEN) {
            if(isalnum(ch)) {
                cur_token += ch;
            }
            else {
                auto final_token = cur_token;
                auto stream = handle_token(cur_token);
                sim_log_debug("Found token:{}", final_token);
                if(stream != final_token) {
                    sim_log_debug("Token translated to:{}", stream);
                    std::vector<char> new_token(stream.begin(), stream.end());
                    preprocess aux_preprocessor(new_token);
                    aux_preprocessor.parse();
                    final_token = aux_preprocessor.get_output();
                }
                output += final_token;
                output += ch;
                start_of_line = false;
                state = PARSER_NORMAL;
            }
        }

        if(!no_advance)
            buffer_index++;
        no_advance = false;
    }
}

std::string_view preprocess::get_output() const {
    return output;
}
