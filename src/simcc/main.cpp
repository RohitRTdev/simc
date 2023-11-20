#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include "debug-api.h"
#include "lib/code-gen.h"
#include "compiler/compile.h"
#include "common/options.h"
#include "common/file-utils.h"

std::string asm_code;

static void start_compilation(std::string_view file_name, const std::vector<char>& file_input) {
    diag::init(file_name, file_input);
    lex(file_input);    
    auto prog = parse();
    eval(std::move(prog));
}

static void dump_buffer(const std::vector<char>& buf) {
    for(auto ch: buf) {
        std::cout << ch;
    }

    std::cout << std::endl;
}

int app_start(int argc, char** argv) {
    
    argparser cmdline(argc, argv, ".s");
    cmdline.parse();
    size_t file_idx = 0;
    for(const auto& file: cmdline.get_input_files()) {
        sim_log_debug("Reading file:{}", file);
        auto file_info_buf = read_file(file);

        sim_log_debug("File size: {}", file_info_buf.size());
        start_compilation(file, file_info_buf);
        
        write_file(cmdline.get_output_files()[file_idx++], asm_code);
    }

    std::cout << "Compilation successful" << std::endl;  
    return 0;
}
