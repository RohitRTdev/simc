#include "preprocessor/preprocess.h"

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

    line_number++;
}

bool preprocess::is_end_of_buf() {
    return !(buffer_index < contents.size());
}

bool preprocess::is_alpha_num() {
    return isalnum(contents[buffer_index]) || contents[buffer_index] == '_';
}

bool preprocess::is_alpha_num(char ch) {
    return isalnum(ch) || ch == '_';
}

bool preprocess::is_valid_macro(std::string_view ident) {
    for(const auto el: ident) {
        if(!is_alpha_num(el)) {
            return false;
        }
    }

    return is_valid_ident(ident);
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

std::pair<std::string, size_t> preprocess::read_next_token(std::string_view line) {
    size_t idx = 0;
    std::string word;
    while(idx < line.size()) {
        while(idx < line.size() && is_white_space(line[idx])) {
            idx++;
        }

        while(idx < line.size() && is_alpha_num(line[idx])) {
            word += line[idx++];
        }

        break;
    }

    return make_pair(word, idx);
}

bool preprocess::is_alpha_numeric_token(std::string_view token) {
    for(int i = token.size()-1; i >= 0; i--) {
        if(is_white_space(token[i])) {
            return true;
        }
        if(!is_alpha_num(token[i])) {
            return false;
        }
    }

    return true;
}

std::string preprocess::trim_whitespace(const std::string& old_token) {
    std::string token(old_token);
    int i = 0;
    for(;i < token.size(); i++) {
        if(!is_white_space(token[i])) {
            break;
        }
    }
    token.erase(0, i);

    for(int i = token.size()-1; i >= 0; i--) {
        if(!is_white_space(token[i])) {
            if(i != token.size()-1) {
                token.erase(i+1, token.size()-i-1);
            }
            break;
        }
    }

    return token;
}

bool preprocess::is_white_space() {
    return contents[buffer_index] == ' ' || contents[buffer_index] == '\t';
}

bool preprocess::is_white_space(char ch) {
    return ch == ' ' || ch == '\t';
}
