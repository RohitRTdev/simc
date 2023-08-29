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

void scope::redefine_symbol_check(std::string_view symbol) const {
    for(const auto& var: variables) {
        if(var.name == symbol) {
            sim_log_error("\"{}\" redefined", symbol);
        }
    }
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
    redefine_symbol_check(name);
    var_info var{};
    var.name = name;
    var.type = type;
    var.stor_spec = stor_spec;
    var.var_id = id;
    var.is_global = is_global;

    sim_log_debug("Declaring variable:{} of phy_type:{}", name, type.base_type);

    variables.push_back(var); 
}

//void scope::add_function_definition(std::string_view fn_name, Ifunc_translation* fn_intf) {
//
//    sim_log_debug("Adding function definition for fn:{}", fn_name);
//    for(auto& fn: functions) {
//        if(std::get<std::string>(fn.name->value) == fn_name) {
//            if(fn.is_defined)
//                sim_log_error("Function {} redefined", fn_name);
//            
//            fn.is_defined = true;
//            fn.fn_intf = fn_intf;
//            return;
//        }
//    }
//}
