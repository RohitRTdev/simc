#include "preprocessor/sym_table.h"
#include "debug-api.h"

void sym_table::add_symbol(std::string name, bool has_value, bool is_macro, bool is_variadic, std::string value) {
    sim_log_debug("Adding macro:{}, has_symbol:{}, value:{}, is_variadic:{}", name, has_value ? "true" : "false", value, is_variadic);
    sym_info info = { has_value, is_macro, is_variadic};
    if (has_value) {
        info.args.push_back(value);
    }

    values[name] = info;
}

void sym_table::add_macro_args(const std::string& name, std::vector<std::string>& args) {
    CRITICAL_ASSERT(values.contains(name), "add_macro_args called on invalid macro");
    sim_log_debug("Adding args for macro:{}", name);
    size_t idx = 1;
    for(const auto& arg: args) {
        sim_log_debug("Arg {}:{}", idx++, arg);
        values[name].args.push_back(arg);
    }

}

void sym_table::remove_symbol(std::string name) {
    values.erase(name);
}

std::pair<bool, bool> sym_table::has_symbol(const std::string& name) {
    bool contains = values.contains(name);    
    bool has_value = false;
    if(contains) {
        has_value = values[name].has_value;
    }
    return std::make_pair(contains, has_value);
}

//Call this function only if symbol is present in table and if it's object macro
std::string& sym_table::operator [](const std::string& name) {
    CRITICAL_ASSERT(values.contains(name), "[] operator called with no {} macro", name);
    return values[name].args[0];
}

std::pair<bool, bool> sym_table::is_macro(const std::string& name) {
    CRITICAL_ASSERT(values.contains(name), "[] operator called with no {} macro", name);
    auto& val = values[name];
    return std::make_pair(val.is_macro, val.is_variadic);
}

std::vector<std::string>& sym_table::fetch_macro_args(const std::string& name) {
    CRITICAL_ASSERT(values.contains(name), "[] operator called with no {} macro", name);
    return values[name].args;
}
