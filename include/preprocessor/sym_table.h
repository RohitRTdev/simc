#pragma once

#include <string>
#include <optional>
#include <unordered_map>

struct sym_table {
    std::unordered_map<std::string, std::optional<std::string>> values;
public:
    void add_symbol(std::string name, bool has_value = false, std::string value = "");
    void remove_symbol(std::string name);
    std::pair<bool, bool> has_symbol(const std::string& name);
    std::string& operator [](const std::string& name);
};