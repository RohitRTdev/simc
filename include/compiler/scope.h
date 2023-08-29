#pragma once

#include <vector>
#include "lib/code-gen.h"
#include "compiler/token.h"
#include "compiler/type.h"

struct var_info {
    std::string name;
    int var_id;
    bool is_global;
    decl_spec stor_spec;
    type_spec type;
};

class scope {
    std::vector<var_info> variables;
    scope* parent;

public:
    scope() = default;
    scope(scope* parent);
    scope* fetch_parent_scope() const;
    bool is_global_scope() const; 

    void redefine_symbol_check(std::string_view symbol) const;
    var_info& fetch_var_info(std::string_view symbol);
    void add_variable(int id, std::string_view name, const type_spec& type, const decl_spec& stor_spec, bool is_global = false); 
    //void add_function_definition(std::string_view fn_name, Ifunc_translation* fn_intf);
};