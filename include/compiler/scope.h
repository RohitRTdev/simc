#pragma once

#include <variant>
#include <vector>
#include "lib/code-gen.h"
#include "compiler/token.h"
#include "compiler/type.h"

struct var_info {
    std::string_view name;
    int var_id;
    bool is_global;
    decl_spec stor_spec;
    type_spec type;
    bool is_defined;
    std::vector<std::string_view> args;
};

class scope {
    std::vector<var_info> variables;
    scope* parent;

    using tu_intf_type = std::shared_ptr<Itranslation>;
    using fn_intf_type = Ifunc_translation*;

    std::variant<tu_intf_type, fn_intf_type> intf; 

    int declare_variable(const var_info& var); 
    void add_param_variable(int id, std::string_view name, const type_spec& type);
public:
    scope(scope* _parent); 
    scope(scope* _parent, tu_intf_type tu);
    scope(scope* _parent, fn_intf_type fn);
    scope* fetch_parent_scope() const;
    bool is_global_scope() const; 

    bool redefine_symbol_check(std::string_view symbol, const type_spec& type, bool no_redefine = false) const;
    var_info& fetch_var_info(std::string_view symbol);
    void add_variable(int id, std::string_view name, const type_spec& type, const decl_spec& stor_spec, bool is_global = false); 
    std::pair<Ifunc_translation*, scope*> add_function_definition(std::string_view fn_name, const std::vector<std::string_view>& fn_args);
};