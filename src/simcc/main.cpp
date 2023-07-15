#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include "debug-api.h"
#include "lib/code-gen.h"
#include "compiler/token.h"

#define FILE_READ_CHUNK_SIZE 256
using file_info_type = std::pair<char*, size_t>;

void start_compilation(const file_info_type& file_info_buf) {
    
    std::vector<uint8_t> input_file(file_info_buf.second);

    size_t i = 0;
    for(auto& el: input_file)
        el = file_info_buf.first[i++];

    lex(input_file);    
    
}


void dump_buffer(const char* buf, size_t size) {
    for(size_t i = 0; i < size; i++) {
        std::cout << buf[i];
    }

    std::cout << std::endl;
}

file_info_type read_file(const std::string& file_name) {
    std::ifstream file_intf(file_name, std::ios::binary);

    char* f_buf = new char[FILE_READ_CHUNK_SIZE]; 
    size_t i = 0;

    if(!file_intf.is_open()) {
        sim_log_error("File open failed!");
    }

    sim_log_debug("Reading file {}", file_name);
    size_t chunk = 1;
    while(!file_intf.eof()) {
        file_intf.read(&f_buf[i++], 1);

        if(i >= chunk*FILE_READ_CHUNK_SIZE) {
            chunk++;
            char* f_buf_tmp = new char[FILE_READ_CHUNK_SIZE*chunk];
            memcpy(f_buf_tmp, f_buf, (chunk-1)*FILE_READ_CHUNK_SIZE);
            delete[] f_buf;
            f_buf = f_buf_tmp;
        }

    }

    return std::pair<char*, size_t>(f_buf, i-1);
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
        start_compilation(file_info_buf);
    }

    sim_log_debug("Compilation successful");    
    return 0;

}
