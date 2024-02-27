#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cstdio>
#include "driver/frontend.h"
#include "common/options.h"
#include "debug-api.h"



//This is not the best way, but we'll stick with this
static std::string generate_random_name() {
    char buffer[L_tmpnam];
    tmpnam(buffer);

    std::string random_name = buffer;
    return random_name;
}


static argparser init_argparser(int argc, char** argv) {
    argparser cmdline(argc, argv);

    cmdline.add_flag('o', argparser::FILE);
    cmdline.add_flag('I', argparser::FILE);
    cmdline.add_flag('E', argparser::NORMAL);
    
    return cmdline;
}


int app_start(int argc, char** argv) {
    auto cmdline = init_argparser(argc, argv);
    cmdline.parse();

    //Get the current directory of the executable
    std::string file_path = argv[0];
    std::string file_dir = std::filesystem::path(file_path).parent_path().string();
    sim_log_debug("Current file directory is {}", file_dir);
   
    std::string preprocessor_path = (std::filesystem::path(file_dir) / std::filesystem::path("sime")).string();  
    std::string compiler_path = (std::filesystem::path(file_dir) / std::filesystem::path("simcc")).string(); 


    static const std::unordered_map<char, argparser::flag_type> v_compiler_args = {};
    static const std::unordered_map<char, argparser::flag_type> v_preprocessor_args = { {'I', argparser::FILE} };

    sim_log_debug("Preprocessor path:{}", preprocessor_path);
    sim_log_debug("Compiler path:{}", compiler_path);
    std::vector<std::string> preprocessor_args, compiler_args;
    for(const auto& [flag, val]: cmdline.flag_store) {
        if(val) {
           if(v_preprocessor_args.contains(flag)) {
               preprocessor_args.push_back("-" + flag);
               sim_log_debug("Adding preprocessor arg:{}", preprocessor_args[preprocessor_args.size() - 1]);
           }
           else if(v_compiler_args.contains(flag)) {
               compiler_args.push_back("-" + flag); 
               sim_log_debug("Adding compiler arg:{}", compiler_args[compiler_args.size() - 1]);
           }
        }
    }

    for(const auto& [flag, val]: cmdline.name_flag_store) {
        if(val.size() && flag != 'o') {
           if(v_preprocessor_args.contains(flag)) {
               std::string arg;
               for(const auto& v: val) {
                    arg += std::string(" -") + flag + std::string(" ") + v; 
               }
               preprocessor_args.push_back(arg);
               sim_log_debug("Adding preprocessor arg:{}", arg);
           }
           else if(v_compiler_args.contains(flag)) {
               std::string arg;
               for(const auto& v: val) {
                    arg += std::string(" -") + flag + std::string(" ") + v; 
               }
               compiler_args.push_back(arg);
               sim_log_debug("Adding compiler arg:{}", arg);
           }
        }
    }
    
    bool only_preprocess = cmdline.flag_store['E'];
    
    std::vector<std::string> tmp_files;
    if (!only_preprocess) {
        sim_log_debug("Generating random file names for preprocessor output...");
        for (size_t idx = 0; idx < cmdline.get_input_files().size(); idx++) {
            tmp_files.push_back(generate_random_name());
            sim_log_debug("Random name:{} is {}", idx, tmp_files[tmp_files.size() - 1]);
        }
    }
    const std::vector<std::string>& preprocessor_o_files = only_preprocess ? cmdline.get_output_files() : tmp_files; 
    frontend(preprocessor_path, preprocessor_args, cmdline.get_input_files(), preprocessor_o_files).invoke();

    if(!only_preprocess) {
        frontend(compiler_path, compiler_args, tmp_files, cmdline.get_output_files()).invoke();
    }

    sim_log_debug("Driver invocation successful");
    return 0;
}
