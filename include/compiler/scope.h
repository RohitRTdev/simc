#pragma once

#include <vector>
#include "lib/code-gen.h"
#include "compiler/token.h"

class scope {

    struct var_info {
        c_var var;
        int var_id;
    };

    struct fn_info {
        bool is_defined;
        const token* ret_type;
        const token* name;
        std::vector<c_type> arg_list;
        Ifunc_translation* fn_intf;
    };

    std::vector<var_info> variables;
    std::vector<fn_info> functions;

    scope* parent;

public:
    scope() = default;
    scope(scope* parent);

    void redefine_symbol_check(std::string_view symbol);
    int fetch_var_id(std::string_view symbol);
    void add_variable(int id, std::string_view name, c_type type);
    void add_variable(int id, std::string_view name, c_type type, std::string_view value);
    void add_function(const token* name, const token* type, const std::vector<c_type>& arg_list); 
    void add_function_definition(std::string_view fn_name, Ifunc_translation* fn_intf);

};