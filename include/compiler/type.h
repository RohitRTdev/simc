#pragma once

#include <deque>
#include "compiler/token.h"
#include "lib/code-gen.h"

struct decl_spec {
    const token* storage_spec;
    const token* type_spec;
    const token* const_qual, *vol_qual;
    const token* sign_qual;

    bool is_stor_none() const;
    bool is_stor_static() const;
    bool is_stor_extern() const;
    bool is_stor_auto() const;
    bool is_stor_register() const;

    c_type fetch_type_spec() const;
    bool is_signed() const;

    bool is_const() const;
    bool is_volatile() const;
};

struct cv_info {
    bool is_const;
    bool is_volatile;

    bool operator == (const cv_info& info) const;
};

struct type_spec;

struct modifier {
    std::deque<cv_info> ptr_list;
    std::deque<std::string> array_spec;
    std::vector<type_spec> fn_spec;

    bool operator == (const modifier& obj) const;

    bool is_function_mod() const;
    bool is_array_mod() const;
    bool is_pointer_mod() const;
};

struct type_spec {
    c_type base_type;
    bool is_signed;
    cv_info cv;
    bool is_register;

    std::deque<modifier> mod_list;

    type_spec resolve_type() const;
    size_t get_size() const; 
    bool is_pointer_type() const;
    bool is_array_type() const;
    bool is_function_type() const;
    
    bool operator == (const type_spec& type) const;

};