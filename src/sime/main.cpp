#include <iostream>
#include "common/file-utils.h"
#include "common/options.h"
#include "preprocessor/preprocess.h"
#include "preprocessor/parser.h"
#include "debug-api.h"

#if defined(SIMDEBUG) && defined(TEST_PARSER)
static int check_parser() {
    std::vector<std::pair<bool, std::string>> test_cases = {{true, "2+3*17-9/(3&&3) == 46-2"},{true, "2+'\n'+3*17-9/(3&&3)"},
        {true, "3+'a'+9/(2-2)*5"}, {false, "55+'invalid' > ((5-23+100))"}};    
    int exit_code = 0; 
    for(auto& test: test_cases) {
        //Emulate terminal character addition to prevent lexer errors
        if(!test.second.size() || test.second[test.second.size()-1] != '\n') {
            test.second.push_back('\n');
        }
        
        if(test.first == evaluator(test.second)) {
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
#endif

static argparser init_argparser(int argc, char** argv, std::string ext) {
    argparser cmdline(argc, argv, ext);

    cmdline.add_flag('o', argparser::FILE);
    cmdline.add_flag('I', argparser::FILE);
    return cmdline;
}

int app_start(int argc, char** argv) {
#if defined(SIMDEBUG) && defined(TEST_PARSER)
    sim_log_debug("Running parser tests...");
    return check_parser();
#endif

    auto cmdline = init_argparser(argc, argv, ".i");
    cmdline.parse();
    size_t file_idx = 0;
   

    //Setup search directories provided by the user
    if(cmdline.name_flag_store.contains('I')) {
        preprocess::search_directories = cmdline.name_flag_store['I'];
#ifdef SIMDEBUG
        size_t idx = 0;
        sim_log_debug("Listing user search directories...");
        for(const auto& dir: preprocess::search_directories) {
            sim_log_debug("Dir {}:{}", idx, dir);
            idx++;
        }
#endif
    }

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
