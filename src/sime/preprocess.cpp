#include "preprocessor/preprocess.h"
#include "debug-api.h"

sym_table preprocess::table;

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
    if (is_valid_ident(cur_token)) {
        auto stream = expand_token(cur_token);
        sim_log_debug("Found token:{}", final_token);
        if (stream != final_token) {
            sim_log_debug("Token expanded to:{}", stream);
            std::string expanded_stream;
            //Parse new stream into words
            size_t idx = 0;
            std::string word;
            parents.push_back(std::string(cur_token));
            while(idx < stream.size()) {
                while(idx < stream.size() && !is_alpha_num(stream[idx])) {
                    expanded_stream += stream[idx];
                    idx++;
                }

                while(idx < stream.size() && is_alpha_num(stream[idx])) {
                    word += stream[idx];
                    idx++;
                }

                if (word.size()) {
                    //Prevents self referential macro expansion
                    if(is_word_parent(word)) {
                        expanded_stream += word;
                    }
                    else {
                        //Find the expanded version of each token within the expanded token recursively
                        expanded_stream += process_token(word); 
                    }
                }

                if(idx < stream.size()) {
                    expanded_stream += stream[idx];
                }
                word.clear();
                idx++;
            }
            parents.pop_back();
            sim_log_debug("Token translated to:{}", expanded_stream);
            return expanded_stream;
        }
    }

    return final_token;
}

preprocess::preprocess(const std::vector<char>& input, bool handle_directives, 
bool read_single_line) : contents(input), handle_directives(handle_directives),
line_number(1), buffer_index(0), state(PARSER_NORMAL),
no_advance(false), read_single_line(read_single_line)
{}

void preprocess::init_diag(std::string_view name) {
    file_name = name;
    diag_inst.init(name, 1, contents);
}

void preprocess::parse() {
    bool start_of_line = true;
    bool found_escaped_character = false;
    std::string cur_token;
    while (buffer_index < contents.size()) {
        char ch = contents[buffer_index];
        
        if(state == PARSER_NORMAL) {
            if(handle_continued_line()) {
                sim_log_debug("Handling continued line in NORMAL state at line:{}", line_number-1);
                no_advance = false;
                continue;
            }
            else if(ch == '/') {
                state = PARSER_COMMENT;
            }
            else if(ch == '\"') {
                sim_log_debug("Handling string");
                state = PARSER_STRING;
                output += ch;
                start_of_line = false;
            }
            else if(ch == '#' && start_of_line && handle_directives) {
                sim_log_debug("Preprocessor directive detected at line:{}", line_number);
                handle_directive();
            }
            else {
                if(is_end_of_line(false)) {
                    start_of_line = true;
                    no_advance = true;
                    if(read_single_line) {
                        sim_log_debug("EOL detected in read_single_line mode");
                        break;
                    } else {
                        skip_newline(true);
                    }
                    sim_log_debug("Processing line number:{}", line_number);
                }
                else if(is_alpha_num()) {
                    cur_token.clear();
                    state = PARSER_TOKEN;
                    no_advance = true;
                }
                else {
                    output += ch;
                    if(!is_white_space()) {
                        start_of_line = false;
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
                start_of_line = false;
                output += "/";
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
                if(handle_directives) {
                    output += process_token(cur_token);
                }
                else {
                    output += cur_token;
                }
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
