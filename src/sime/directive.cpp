#include "preprocessor/preprocess.h"
#include "debug-api.h"

void preprocess::handle_define(std::string_view dir_line) {
    auto [macro, next_idx] = read_next_word(dir_line);

    //We're only considering object macros currently

    if(!is_valid_ident(macro)) {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Invalid identifier name for macro:{}", macro);
    }

    if(table.has_symbol(macro).first) {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Macro: {} is being redefined", macro);
    }

    bool has_value = false;
    std::string value;
    for(char ch: dir_line.substr(next_idx)) {
        if(!is_white_space(ch)) {
            has_value = true;
            value = dir_line.substr(next_idx);
            break;
        }
    }
    sim_log_debug("Adding macro:{}, has_symbol:{}, value:{}", macro, has_value ? "true" : "false", value);
    table.add_symbol(macro, has_value, value);
}

void preprocess::handle_directive() {
    std::vector<char> rem_line(contents.begin() + buffer_index + 1, contents.end());
    preprocess line_reader_inst(rem_line, false, true);
    line_reader_inst.parse();

    sim_log_debug("Preprocess line read is:{}", line_reader_inst.get_output());
    dir_line_start_idx = buffer_index;
    buffer_index += line_reader_inst.buffer_index;
    line_number += line_reader_inst.line_number - 1;

    auto [directive, next_idx] = read_next_word(line_reader_inst.get_output());

    if(directive == "define") {
        handle_define(line_reader_inst.get_output().substr(next_idx));
    }
    else {
        diag_inst.print_error(dir_line_start_idx);
        sim_log_error("Unrecognized preprocessor directive:{}", directive);
    }


}

