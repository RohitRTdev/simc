#include "common/options.h"
#include "debug-api.h"

std::string argparser::generate_default_file_name(std::string_view file_path) {
    auto remove_suffix = [] (const std::string& file_name) {
        std::string _file = file_name;
        size_t offset = _file.find_last_of('.');
        if(offset == std::string::npos)
            return _file;
        
        _file.erase(_file.begin() + offset, _file.end());
        return _file;
    };

    std::string output_file_name = std::string(file_path);
    size_t lastSlash = file_path.find_last_of("/\\");
    
    if (lastSlash == std::string::npos) {
        output_file_name = remove_suffix(output_file_name) + ".s";
        return output_file_name;
    }
    
    return remove_suffix(output_file_name.substr(lastSlash + 1)) + ".s";
}

argparser::argparser(int argc, char** argv) : m_argc(argc), m_argv(argv)
{}

void argparser::parse() {
    int i = 1;
    while (i < m_argc) 
    {
        std::string arg = m_argv[i];
        if (arg == "-o") {
            if(i + 1 >= m_argc || std::string(m_argv[i+1]) == "" || std::string(m_argv[i+1])[0] == '-') {
                sim_log_error("-o option must be followed by a valid filename argument");
            }
            output_files.push_back(m_argv[i + 1]);
            i += 2;
        } else {
            input_files.push_back(arg);
            i++;
        }
    }

    if(output_files.size() > input_files.size()) {
        sim_log_error("More output files mentioned than input files");
    }

    if(!input_files.size()) {
        sim_log_error("Please mention atleast one input file");
    }

    size_t diff = input_files.size() - output_files.size();
    for(i = 0; i < diff; i++)
        output_files.push_back(generate_default_file_name(input_files[output_files.size() + i]));
}

const std::vector<std::string>& argparser::get_output_files() const {
    return output_files;
}

const std::vector<std::string>& argparser::get_input_files() const {
    return input_files;
}

