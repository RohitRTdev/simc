#include <iostream>
#include "common/file-utils.h"
#include "debug-api.h"

int app_start(int argc, char** argv) {

    if(argc < 2) {
        sim_log_error("Please input atleast one file..");
    }

    auto file_contents = read_file(argv[1]);
    sim_log_debug("Printing file contents");
    for(auto ch: file_contents)
        std::cout << ch;
    std::cout << std::endl;
    sim_log_debug("Finished printing file..");    

    return 0;
}