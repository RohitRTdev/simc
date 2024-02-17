#include <iostream>
#include "common/file-utils.h"
#include "common/options.h"
#include "preprocessor/preprocess.h"
#include "debug-api.h"


static argparser init_argparser(int argc, char** argv, std::string ext) {
    argparser cmdline(argc, argv, ext);

    cmdline.add_flag('o', argparser::FILE);

    return cmdline;
}

int app_start(int argc, char** argv) {
    auto cmdline = init_argparser(argc, argv, ".i");
    cmdline.parse();
    size_t file_idx = 0;

    for(const auto& file: cmdline.get_input_files()) {
        preprocess::init_with_defaults(file);
        auto file_info_buf = *read_file(file);

        preprocess main_preprocessor(file_info_buf);
        main_preprocessor.init_diag(file);
        main_preprocessor.parse();
        write_file(cmdline.get_output_files()[file_idx++], main_preprocessor.get_output());
    }

    sim_log_debug("Preprocessing successful");

    return 0;
}
