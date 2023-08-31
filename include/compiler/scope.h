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
    using fn_intf_type = std::shared_ptr<Ifunc_translation>;

    std::variant<tu_intf_type, fn_intf_type> intf; 

    c_type fetch_phy_type(const var_info& var); 
    int declare_variable(const var_info& var); 
public:
    scope(scope* _parent); 
    scope(scope* _parent, std::shared_ptr<Itranslation> tu);
    scope(scope* _parent, std::shared_ptr<Ifunc_translation> fn);
    scope* fetch_parent_scope() const;
    bool is_global_scope() const; 

    bool redefine_symbol_check(std::string_view symbol, const type_spec& type) const;
    var_info& fetch_var_info(std::string_view symbol);
    void add_variable(int id, std::string_view name, const type_spec& type, const decl_spec& stor_spec, bool is_global = false); 
    Ifunc_translation* add_function_definition(std::string_view fn_name, const std::vector<std::string_view>& fn_args);
};