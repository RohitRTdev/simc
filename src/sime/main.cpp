#include <iostream>
#include "common/file-utils.h"
#include "common/options.h"
#include "preprocessor/preprocess.h"
#include "debug-api.h"

int app_start(int argc, char** argv) {
    argparser cmdline(argc, argv, ".i");
    cmdline.parse();
    size_t file_idx = 0;
    preprocess::init_with_defaults();
    
    for(const auto& file: cmdline.get_input_files()) {
        sim_log_debug("Reading file:{}", file);
        auto file_info_buf = read_file(file);

        sim_log_debug("File size: {}", file_info_buf.size());
        preprocess main_preprocessor(file_info_buf);
        main_preprocessor.parse();
        write_file(cmdline.get_output_files()[file_idx++], main_preprocessor.get_output());
    }
   
    sim_log_debug("Preprocessing successful");    

    return 0;
}