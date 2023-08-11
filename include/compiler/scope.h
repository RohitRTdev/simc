#pragma once

#include <vector>
#include "lib/code-gen.h"
#include "compiler/token.h"

struct var_info {
    c_var var;
    int var_id;
    bool is_global;
};

struct fn_info {
    bool is_defined;
    const token* ret_type;
    const token* name;
    std::vector<c_type> arg_list;
    Ifunc_translation* fn_intf;
};

class scope {

    std::vector<var_info> variables;
    std::vector<fn_info> functions;

    scope* parent;

public:
    scope() = default;
    scope(scope* parent);
    scope* fetch_parent_scope();

    void redefine_symbol_check(std::string_view symbol);
    var_info& fetch_var_info(std::string_view symbol);
    void add_variable(int id, std::string_view name, c_type type, bool is_global = false);
    void add_variable(int id, std::string_view name, c_type type, std::string_view value, bool is_global = false);
    void add_function(const token* name, const token* type, const std::vector<c_type>& arg_list); 
    void add_function_definition(std::string_view fn_name, Ifunc_translation* fn_intf);

};