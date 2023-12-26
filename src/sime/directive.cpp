#include "preprocessor/preprocess.h"
#include "debug-api.h"

void preprocess::handle_directive() {
    std::vector<char> rem_line(contents.begin() + buffer_index + 1, contents.end());
    preprocess line_reader_inst(rem_line, false, true);
    line_reader_inst.parse();

    sim_log_debug("Preprocess line read is:{}", line_reader_inst.get_output());
    buffer_index += line_reader_inst.buffer_index;    

    


}

