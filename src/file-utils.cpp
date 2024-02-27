#include <fstream>
#include "common/file-utils.h"
#include "debug-api.h"

#define FILE_READ_CHUNK_SIZE 256

std::optional<std::vector<char>> read_file(std::string_view file_name, bool exit_on_error) {
    std::ifstream file_intf(std::string(file_name), std::ios::binary);

    std::optional<std::vector<char>> f_buf_wrap;
    std::vector<char> f_buf(FILE_READ_CHUNK_SIZE); 
    size_t i = 0;

    if(!file_intf.is_open()) {
        if(exit_on_error) {
            sim_log_error("File open failed!");
        }

        return f_buf_wrap;
    }

    sim_log_debug("Reading file {}", file_name);
    size_t chunk = 1;
    while(!file_intf.eof()) {
        file_intf.read(&f_buf[i], FILE_READ_CHUNK_SIZE);

        sim_log_debug("gcount()={}", file_intf.gcount());
        //We have more chunks to read

        if (file_intf.gcount() == 0) {
            //We're likely reading some non regular file(device file, tty, pipe, directory etc)
            if (exit_on_error) {
                sim_log_error("File open failed!");
            }

            return f_buf_wrap;
        }
        if(file_intf.gcount() == FILE_READ_CHUNK_SIZE) {
            chunk++;
            f_buf.resize(chunk*FILE_READ_CHUNK_SIZE);
        }
        else {
            f_buf.resize((chunk-1)*FILE_READ_CHUNK_SIZE + file_intf.gcount());
        }
        i += FILE_READ_CHUNK_SIZE;
    }
        
    sim_log_debug("File size: {}", f_buf.size());
    f_buf_wrap = f_buf;
    return f_buf_wrap;
}

void write_file(std::string_view name, std::string_view code) {
    std::ofstream file_intf(std::string(name), std::ios::binary);

    if(!file_intf.is_open()) {
        sim_log_error("Could not create output file:{}", name);
    }

    sim_log_debug("Writing output to file:{}", name);
    file_intf.write(code.data(), code.size());
}
