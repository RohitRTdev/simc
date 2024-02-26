#include <iostream>
#include "common/file-utils.h"
#include "common/options.h"
#include "preprocessor/preprocess.h"
#include "preprocessor/parser.h"
#include "debug-api.h"


#define TEST_PARSER
static int check_parser() {
    std::vector<std::pair<bool, std::string>> test_cases = {{true, "2+3*17-9/(3&&3) == 4-2"}};    
    int exit_code = 0; 
    for(auto& test: test_cases) {
        if(!test.second.size() || test.second[test.second.size()-1] != '\n') {
            test.second.push_back('\n');
        }
        
        if(test.first == eval_expr(test.second)) {
            sim_log_debug("TEST PASSED");
        }
        else {
            sim_log_debug("TEST FAILED");
            sim_log_debug("Exp:{}", test.second);
            exit_code = -1;
        }
    }
    return exit_code;
}

static argparser init_argparser(int argc, char** argv, std::string ext) {
    argparser cmdline(argc, argv, ext);

    cmdline.add_flag('o', argparser::FILE);

    return cmdline;
}

int app_start(int argc, char** argv) {

#ifdef TEST_PARSER
    return check_parser();
#endif

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
