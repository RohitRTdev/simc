#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include "debug-api.h"
#include "compiler/code-gen.h"

#define FILE_READ_CHUNK_SIZE 256

void dump_buffer(const char* buf, size_t size) {
    for(size_t i = 0; i < size; i++) {
        std::cout << buf[i];
    }

    std::cout << std::endl;
}

std::pair<char*, size_t> read_file(const std::string& file_name) {
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

    auto file_info = read_file(files[0]);

    std::unique_ptr<Ifunc_translation> unit = std::unique_ptr<Ifunc_translation>(create_function_translation_unit());

    //a = a + b + 5; b = b + 10; my_bad = my_bad + b;
    int var1 = unit->declare_local_variable("a", C_INT);
    int var2 = unit->declare_local_variable("b", C_INT);
    int res1 = unit->add_ii(var1, var2);
    unit->add_rc(res1, 5);
    int res2 = unit->add_ic(var2, 10);
    int var3 = unit->declare_local_variable("my_bad", C_INT);
    int res3 = unit->add_ii(var2, var3);
    int res4 = unit->add_rr(res1, res2);
    int res5 = unit->add_rr(res4, res3);
    int res6 = unit->add_rr(res4, res5);
    sim_log_debug("Adding res6 and res1");
    int res7 = unit->add_rr(res6, res1);
    int res8 = unit->add_rr(res3, res6);
    int res9 = unit->add_rr(res1, res2);
    unit->assign_ir(var3, res8);

    unit->generate_code();
    auto code = unit->fetch_code();

    sim_log_debug("Printing generated code...");
    sim_log_debug("\n" + code);

    std::cout << code << std::endl;
    return 0;

}
