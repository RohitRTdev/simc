#include <variant>
#include "compiler/scope.h"
#include "debug-api.h"

scope::scope(scope* _parent) : parent(_parent)
{}

scope* scope::fetch_parent_scope() const {
    return parent;
}

bool scope::is_global_scope() const {
    return !parent;
}

bool scope::redefine_symbol_check(std::string_view symbol, const type_spec& type) const {
    for(auto& var: variables) {
        if(var.name == symbol) {
            if(!var.is_defined) {
                if(!(var.type == type)) {
                    sim_log_error("Current definition of symbol:{} does not match earlier declaration", symbol);
                }
                return true;
            }
            else {
                sim_log_error("\"{}\" redefined", symbol);
            }
        }
    }

    return false;
}

var_info& scope::fetch_var_info(std::string_view symbol) {
    scope* search_scope = this;
    sim_log_debug("Searching for symbol:{}", symbol);
    while(search_scope) {
        for(auto& var: search_scope->variables) {
            if(var.name == symbol) {
                sim_log_debug("Found var_id:{} for symbol:{}", var.var_id, var.name);
                return var;
            }
        }
        sim_log_debug("Couldn't find in current scope. Moving up a scope");
        search_scope = search_scope->parent;
    }

    sim_log_error("Variable:{} seems to be undefined", symbol);
}

void scope::add_variable(int id, std::string_view name, const type_spec& type, const decl_spec& stor_spec, bool is_global) {
    if(redefine_symbol_check(name, type)) {
        sim_log_debug("Found existing declaration for symbol");
        auto& var = fetch_var_info(name);
        var.is_defined = true;
    }
    else {
        var_info var{};
        var.name = name;
        var.type = type;
        var.stor_spec = stor_spec;
        var.var_id = id;
        var.is_global = is_global;

        if(!(stor_spec.is_stor_extern() || (type.is_function_type() && stor_spec.is_stor_none()))) {
            var.is_defined = true;
            sim_log_debug("Normal declaration of symbol:{}", name);
        }
        else {
            sim_log_debug("Extern declaration of symbol:{}", name);
        }

        variables.push_back(var);
    }
}

void scope::add_function_definition(std::string_view fn_name, const type_spec& type, const decl_spec& stor_spec, const std::vector<std::string_view>& fn_args) {
    add_variable(0, fn_name, type, stor_spec, true);
    auto& fn = fetch_var_info(fn_name);
    fn.args = fn_args;
}
