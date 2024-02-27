#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>

struct sym_table {

    struct sym_info {
        bool has_value;
        bool is_macro;
        bool is_variadic;
        std::vector<std::string> args;
    };
    std::unordered_map<std::string, sym_info> values;
public:
    void add_symbol(std::string name, bool has_value = false, bool is_macro = false, 
    bool is_variadic = false, std::string value = "");
    void add_macro_args(const std::string& name, std::vector<std::string>& args);
    void remove_symbol(std::string name);
    std::pair<bool, bool> has_symbol(const std::string& name);
    std::string& operator [](const std::string& name);
    std::vector<std::string>& fetch_macro_args(const std::string& name); 
    std::pair<bool, bool> is_macro(const std::string& name);
};
