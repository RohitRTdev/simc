#include <optional>
#include <variant>
#include "compiler/scope.h"
#include "debug-api.h"

scope::scope(scope* _parent) : parent(_parent), intf(_parent->intf)
{}

scope::scope(scope* _parent, tu_intf_type tu) : parent(_parent), intf(tu)
{}

scope::scope(scope* _parent, fn_intf_type fn) : parent(_parent), intf(fn)
{}

scope* scope::fetch_parent_scope() const {
    return parent;
}

bool scope::is_global_scope() const {
    return !parent;
}

int scope::declare_variable(const var_info& var) {
    CRITICAL_ASSERT(!var.type.is_function_type(), "declare_variable() called with function type");

    std::optional<size_t> mem_var_size;
    if(var.type.is_array_type()) {
        mem_var_size = var.type.get_size();
        sim_log_debug("Declaring array type of size:{}", mem_var_size.value());
    } 
    auto [phy_type, is_signed] = var.type.get_simple_type(); 
    if(is_global_scope()) {
        if(mem_var_size) {
            return std::get<tu_intf_type>(intf)->declare_global_mem_variable(var.name, var.stor_spec.is_stor_static(), mem_var_size.value());
        }
        else {
            return std::get<tu_intf_type>(intf)->declare_global_variable(var.name, phy_type, is_signed, var.stor_spec.is_stor_static());
        }
    }
    else {
        if(mem_var_size) {
            return std::get<fn_intf_type>(intf)->declare_local_mem_variable(var.name, mem_var_size.value());
        }
        else {
            return std::get<fn_intf_type>(intf)->declare_local_variable(var.name, phy_type, is_signed);
        }
    }
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

        if(!var.type.is_function_type())
            var.var_id = declare_variable(var);
    }
    else {
        var_info var{};
        var.name = name;
        var.type = type;
        var.stor_spec = stor_spec;
        var.var_id = id;
        var.is_global = is_global;

        if(stor_spec.is_stor_extern() || type.is_function_type()) {
            sim_log_debug("Extern declaration of symbol:{}", name);
        }
        else { 
            var.is_defined = true;
            sim_log_debug("Normal declaration of symbol:{}", name);
            if(!type.is_function_type())
                var.var_id = declare_variable(var);
        }

        variables.push_back(var);
    }
}

Ifunc_translation* scope::add_function_definition(std::string_view fn_name, const std::vector<std::string_view>& fn_args) {
    auto& fn = fetch_var_info(fn_name);
    fn.is_defined = true;
    fn.args = fn_args;

    auto [base_type, is_signed] = fn.type.resolve_type().get_simple_type();

    return std::get<tu_intf_type>(intf)->add_function(fn_name, base_type, is_signed, fn.stor_spec.is_stor_static());
}
