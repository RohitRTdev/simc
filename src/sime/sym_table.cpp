#include "preprocessor/sym_table.h"

void sym_table::add_symbol(std::string name, bool has_value, std::string value) {
    std::optional<std::string> _value;
    if(has_value)
        _value = value;
    values[name] = _value;
}

void sym_table::remove_symbol(std::string name) {
    values.erase(name);
}

std::pair<bool, bool> sym_table::has_symbol(const std::string& name) {
    bool contains = values.contains(name);    
    bool has_value = false;
    if(contains) {
        has_value = values[name].has_value();
    }
    return std::make_pair(contains, has_value);
}

//Call this function only if symbol is present in table
std::string& sym_table::operator [](const std::string& name) {
    return *values[name];
}
