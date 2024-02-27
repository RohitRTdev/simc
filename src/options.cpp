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
        output_file_name = remove_suffix(output_file_name) + m_extension;
        return output_file_name;
    }
    
    return remove_suffix(output_file_name.substr(lastSlash + 1)) + m_extension;
}

argparser::argparser(int argc, char** argv, std::string_view extension) : m_argc(argc), m_argv(argv),
m_extension(extension), gen_def(true)
{}

argparser::argparser(int argc, char** argv) : m_argc(argc), m_argv(argv),
gen_def(false)
{}

void argparser::add_flag(char ch, flag_type type) {
    CRITICAL_ASSERT(!flag_store.contains(ch) && !name_flag_store.contains(ch), "Flag -{} is already added");

    if(type == NORMAL) {
        flag_store.insert(std::make_pair(ch, false));   
    }
    else {
        name_flag_store.insert(std::make_pair(ch, std::vector<std::string>()));
    }

}

void argparser::parse() {
    int i = 1;
    while (i < m_argc) 
    {
        std::string arg = m_argv[i];
        //It's an option
        if(arg[0] == '-') {
            if(arg.size() != 2 || (!flag_store.contains(arg[1]) && !name_flag_store.contains(arg[1]))) {
                sim_log_error("Unrecognized option {}", arg);
            }

            if(flag_store.contains(arg[1])) {
                if(flag_store[arg[1]]) {
                    sim_log_error("Option {} cannot be used multiple times");
                }
                flag_store[arg[1]] = true;
            }
            else {
                if(i >= m_argc - 1) {
                    sim_log_error("Invalid use of {} option", arg);
                }

                std::string val = m_argv[++i];
                if(val[0] == '-') {
                    sim_log_error("Invalid value:{} given for option {}", val, arg);
                }

                name_flag_store[arg[1]].push_back(val);
            }


        }
        else {
            input_files.push_back(arg);
        }
        i++;
    }

    CRITICAL_ASSERT(name_flag_store.contains('o'), "argparser doesn't contain -o option configured");

    //Special treatment for -o option as it's used in all modules
    size_t user_op_files = name_flag_store['o'].size();
    for(i = 0; i < user_op_files; i++) {
        output_files.push_back(name_flag_store['o'][i]);
    }

    if(gen_def) {
        for(i = user_op_files; i < input_files.size(); i++) {
            output_files.push_back(generate_default_file_name(input_files[i]));
        }
    }

    if(output_files.size() > input_files.size()) {
        sim_log_error("More output files mentioned than input files");
    }

    if(!input_files.size()) {
        sim_log_error("Please mention atleast one input file");
    }
}

const std::vector<std::string>& argparser::get_output_files() const {
    return output_files;
}

const std::vector<std::string>& argparser::get_input_files() const {
    return input_files;
}

