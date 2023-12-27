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
    token::global_diag_inst.init(file_name, 1, file_input);
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

static argparser init_argparser(int argc, char** argv, std::string ext) {
    argparser cmdline(argc, argv, ext);

    cmdline.add_flag('o', argparser::FILE);

    return cmdline;
}


int app_start(int argc, char** argv) {
    
    auto cmdline = init_argparser(argc, argv, ".s");
    cmdline.parse();
    size_t file_idx = 0;
    for(const auto& file: cmdline.get_input_files()) {
        auto file_info_buf = read_file(file);

        start_compilation(file, file_info_buf);
        write_file(cmdline.get_output_files()[file_idx++], asm_code);
    }

    std::cout << "Compilation successful" << std::endl;  
    return 0;
}
