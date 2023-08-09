#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include "debug-api.h"
#include "lib/code-gen.h"
#include "compiler/compile.h"

#define FILE_READ_CHUNK_SIZE 256

std::string asm_code;

static void start_compilation(const std::vector<char>& file_input) {

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

std::vector<char> read_file(const std::string& file_name) {
    std::ifstream file_intf(file_name, std::ios::binary);

    std::vector<char> f_buf(FILE_READ_CHUNK_SIZE); 
    size_t i = 0;

    if(!file_intf.is_open()) {
        sim_log_error("File open failed!");
    }

    sim_log_debug("Reading file {}", file_name);
    size_t chunk = 1;
    while(!file_intf.eof()) {
        file_intf.read(&f_buf[i], FILE_READ_CHUNK_SIZE);

        sim_log_debug("gcount()={}", file_intf.gcount());
        //We have more chunks to read
        if(file_intf.gcount() == FILE_READ_CHUNK_SIZE) {
            chunk++;
            f_buf.resize(chunk*FILE_READ_CHUNK_SIZE);
        }
        else {
            f_buf.resize((chunk-1)*FILE_READ_CHUNK_SIZE + file_intf.gcount());
        }
        i += FILE_READ_CHUNK_SIZE;
    }

    return f_buf;
}

static void write_file(const std::string& name, std::string_view code) {
    std::ofstream file_intf(name, std::ios::binary);

    if(!file_intf.is_open()) {
        sim_log_error("Could not create output file:{}", name);
    }

    file_intf.write(code.data(), code.size());
}

int app_start(int argc, char** argv) {
    
    if(argc <= 1) {
        sim_log_error("Please input atleast 1 file..");
        return -1;
    }

    std::vector<std::string> files(argc-1);
    for (int i = 0; i < argc - 1; i++) {
        files[i] = argv[i+1];
        sim_log_debug("File {}: {}", i+1, files[i]);
    }

    for(auto& file: files) {
        auto file_info_buf = read_file(file);
        sim_log_debug("File size: {}", file_info_buf.size());
        start_compilation(file_info_buf);
        write_file("out.s", asm_code);
    }

    std::cout << "Compilation successful" << std::endl;  

    return 0;
}
