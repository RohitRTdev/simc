#pragma once

#include <variant>
#include <vector>
#include "lib/code-gen.h"
#include "compiler/token.h"
#include "compiler/type.h"
#include "compiler/ast.h"

struct var_info {
    std::string_view name;
    int var_id;
    bool is_global;
    decl_spec stor_spec;
    type_spec type;
    bool is_defined;
    bool is_body_defined;
    bool is_initialized;
    std::vector<std::string_view> args;
    std::string init_value; 
};

class scope {
    std::vector<var_info> variables;
    scope* parent;
    const token* cur_var_token = nullptr;
    using tu_intf_type = std::shared_ptr<Itranslation>;
    using fn_intf_type = Ifunc_translation*;

    std::variant<tu_intf_type, fn_intf_type> intf; 

    int declare_variable(const var_info& var); 
    void add_param_variable(int id, std::string_view name, const type_spec& type, const token* key_token);
    void initialize_variable(var_info& var, std::unique_ptr<ast> init_expr);
    void print_error() const;
public:
    static int static_id;
    
    scope(scope* _parent); 
    scope(scope* _parent, tu_intf_type tu);
    scope(scope* _parent, fn_intf_type fn);
    scope* fetch_parent_scope() const;
    bool is_global_scope() const; 

    bool redefine_symbol_check(std::string_view symbol, const type_spec& type, const decl_spec& stor_spec, bool no_redefine = false) const;
    var_info& fetch_var_info(std::string_view symbol, const token* key_token);
    void add_variable(std::string_view name, const type_spec& type, const decl_spec& stor_spec, const token* key_token, std::unique_ptr<ast> init_expr = std::unique_ptr<ast>(), bool is_global = false); 
    int add_string_constant(std::string_view name, std::string_view value);
    std::pair<Ifunc_translation*, scope*> add_function_definition(std::string_view fn_name, 
    const std::vector<std::string_view>& fn_args,
    const std::vector<const token*>& fn_arg_tokens);
};