#include <variant>
#include "compiler/scope.h"
#include "debug-api.h"

scope::scope(scope* _parent) : parent(_parent)
{}

void scope::redefine_symbol_check(std::string_view symbol) {
    for(const auto& var: variables) {
        if(var.var.name == symbol) {
            sim_log_error("\"{}\" redefined", symbol);
        }
    }

    for(const auto& fn: functions) {
        if(std::get<std::string>(fn.name->value) == symbol) {
            sim_log_error("\"{}\" redefined", symbol);
        }
    }
}


int scope::fetch_var_id(std::string_view symbol) {
    
    scope* search_scope = this;
    sim_log_debug("Searching for symbol:{}", symbol);
    while(search_scope) {
        for(const auto& var: search_scope->variables) {
            if(var.var.name == symbol) {
                return var.var_id;
            }

        }

        sim_log_debug("Couldn't find in current scope. Moving up a scope");
        search_scope = search_scope->parent;
    }

    CRITICAL_ASSERT_NOW("fetch_var_id() called with invalid symbol name:{}", symbol);
    return 0;
}

void scope::add_variable(int id, std::string_view name, c_type type) {
    redefine_symbol_check(name);
    scope::var_info var{};
    var.var.name = name;
    var.var.type = type;

    var.var_id = id;

    sim_log_debug("Declaring variable:{} of type:{}", name, type);

    variables.push_back(var); 
}

void scope::add_variable(int id, std::string_view name, c_type type, std::string_view value) {
    add_variable(id, name, type);
    variables[variables.size()-1].var.value = value;
}

void scope::add_function(const token* name, const token* type, const std::vector<c_type>& arg_list) {
    redefine_symbol_check(std::get<std::string>(name->value));
    scope::fn_info fn{false, type, name, arg_list};

    sim_log_debug("Declaring function:{}", std::get<std::string>(name->value));
    functions.push_back(fn);
}

void scope::add_function_definition(std::string_view fn_name, Ifunc_translation* fn_intf) {

    sim_log_debug("Adding function definition for fn:{}", fn_name);
    for(auto& fn: functions) {
        if(std::get<std::string>(fn.name->value) == fn_name) {
            if(fn.is_defined)
                sim_log_error("Function {} redefined", fn_name);
            
            fn.is_defined = true;
            fn.fn_intf = fn_intf;
            return;
        }
    }
}
