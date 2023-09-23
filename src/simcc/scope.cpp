#include <optional>
#include <variant>
#include "compiler/scope.h"
#include "compiler/eval.h"
#include "compiler/utils.h"
#include "debug-api.h"

int scope::static_id = 0;

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
            return std::get<tu_intf_type>(intf)->declare_global_mem_variable(var.name, var.stor_spec.is_stor_static(), *mem_var_size);
        }
        else {
            return std::get<tu_intf_type>(intf)->declare_global_variable(var.name, phy_type, is_signed, var.stor_spec.is_stor_static());
        }
    }
    else {
        if(mem_var_size) {
            return std::get<fn_intf_type>(intf)->declare_local_mem_variable(var.name, var.stor_spec.is_stor_static(), *mem_var_size);
        }
        else {
            return std::get<fn_intf_type>(intf)->declare_local_variable(var.name, phy_type, is_signed, var.stor_spec.is_stor_static());
        }
    }
}

void scope::initialize_variable(var_info& var, std::unique_ptr<ast> init_expr) {
    if(var.type.is_function_type()) {
        sim_log_error("Function type cannot be initialized");
    }

    CRITICAL_ASSERT(!var.type.is_array_type(), "Array type initialization is not supported right now");

    if(var.stor_spec.is_stor_extern()) {
        sim_log_error("Extern declaration cannot be initialized");
    }

    if(var.init_value != "") {
        sim_log_error("Redefinition of variable:{}", var.name);
    }
    sim_log_debug("Initializing variable with var_id:{}", var.var_id);
    if(is_global_scope() || var.stor_spec.is_stor_static()) {
        //Expr must be computable at compile time
        code_gen::eval_only = true;
        auto res = eval_expr(std::move(init_expr), nullptr, this, true).eval();
        if(!res.is_constant) {
            sim_log_error("Init expression is not compile time computable");
        }
        code_gen::eval_only = false;
        var.init_value = res.constant;
        if(!var.type.is_pointer_type() && res.type.is_pointer_type()) {
            sim_log_error("Cannot assign pointer type to non pointer type at global scope");
        }

        if (is_global_scope())
            std::get<tu_intf_type>(intf)->init_variable(var.var_id, var.init_value);
        else
            std::get<fn_intf_type>(intf)->init_variable(var.var_id, var.init_value);
    }
    else {
        auto res = eval_expr(std::move(init_expr), std::get<1>(intf), this).eval();

        if(!var.type.is_type_convertible(res.type)) {
            sim_log_error("Cannot convert init-expr type to variable type");
        }
        
        res.expr_id = type_spec::convert_type(var.type, res.expr_id, res.type, std::get<1>(intf), !res.is_constant);
        if (res.is_constant) {
            code_gen::call_code_gen(std::get<1>(intf), &Ifunc_translation::assign_var, var.var_id, res.constant);
        }
        else {
            code_gen::call_code_gen(std::get<1>(intf), &Ifunc_translation::assign_var, var.var_id, res.expr_id);
        }
        res.free(std::get<1>(intf));
    }
    
    var.is_initialized = true;
}


bool scope::redefine_symbol_check(std::string_view symbol, const type_spec& type, const decl_spec& stor_spec, bool no_redefine) const {
    for(auto& var: variables) {
        if(var.name == symbol) {
            if(!no_redefine && (stor_spec.is_stor_extern() || type.is_function_type() || 
            (stor_spec.is_stor_static() && (var.stor_spec.is_stor_static() || var.stor_spec.is_stor_extern())) || !var.is_defined)) {
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

void scope::add_variable(std::string_view name, const type_spec& type, const decl_spec& stor_spec,
std::unique_ptr<ast> init_expr, bool is_global) {
    if(redefine_symbol_check(name, type, stor_spec)) {
        sim_log_debug("Found existing declaration for symbol");
        auto& var = fetch_var_info(name);
        var.is_defined = true;

        if(!var.type.is_function_type())
            var.var_id = declare_variable(var);
        
        if(init_expr) {
            initialize_variable(var, std::move(init_expr));
        }
    }
    else {
        var_info var{};
        var.name = name;
        var.type = type;
        var.stor_spec = stor_spec;
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
        if(init_expr) {
            initialize_variable(variables.back(), std::move(init_expr));
        }
    }
}

void scope::add_param_variable(int id, std::string_view name, const type_spec& type) {
    redefine_symbol_check(name, type, decl_spec{}, true);
    var_info var{};
    var.name = name;
    var.type = type;
    var.var_id = id;

    variables.push_back(var);
}

int scope::add_string_constant(std::string_view name, std::string_view value) {
    int id = 0;
    if(is_global_scope()) {
        id = std::get<tu_intf_type>(intf)->declare_string_constant(name, value);
    }
    else {
        id = std::get<fn_intf_type>(intf)->declare_string_constant(name, value);
    }

    return id;
}

std::pair<Ifunc_translation*, scope*> scope::add_function_definition(std::string_view fn_name, const std::vector<std::string_view>& fn_args) {
    auto& fn = fetch_var_info(fn_name);
    fn.is_defined = true;
    fn.args = fn_args;

    auto [base_type, is_signed] = fn.type.resolve_type().get_simple_type();

    auto fn_intf = std::get<tu_intf_type>(intf)->add_function(fn_name, base_type, is_signed, fn.stor_spec.is_stor_static());
    auto fn_scope = new scope(this, fn_intf);

    //Declare the argument list into the new function scope
    auto arg_index = 0;
    for(const auto& arg: fn.type.mod_list[0].fn_spec) {
        type_spec tmp_arg = arg;
        if(tmp_arg.is_function_type() || tmp_arg.is_array_type()) {
            tmp_arg.convert_to_pointer_type();
        }
        auto [sim_type, is_sign] = arg.get_simple_type();
        int id = fn_intf->save_param(fn_args[arg_index], sim_type, is_sign);
        fn_scope->add_param_variable(id, fn.args[arg_index], tmp_arg);
        arg_index++;
    }

    return std::make_pair(fn_intf, fn_scope);
}
