#pragma once

#include <deque>
#include "core/token.h"
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

    void print_error() const;
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

    modifier() = default;
    modifier(cv_info) {
        ptr_list.push_back(cv_info{});
    }

    modifier(size_t size) {
        array_spec.push_back(std::to_string(size));
    } 

    bool operator == (const modifier& obj) const;

    bool is_function_mod() const;
    bool is_array_mod() const;
    bool is_pointer_mod() const;
};

using phy_type = std::pair<int, type_spec>;

struct type_spec {
    c_type base_type;
    bool is_signed;
    cv_info cv;
    bool is_register;

    std::deque<modifier> mod_list;

    type_spec resolve_type() const;
    type_spec addr_type() const;
    size_t get_size() const; 
    bool is_pointer_type() const;
    bool is_array_type() const;
    bool is_function_type() const;
    bool is_integral() const;
    bool is_pointer_to_function() const;

    void convert_to_pointer_type();
    size_t get_pointer_base_type_size() const;
    bool is_void() const;
    bool is_modified_type() const;
    bool is_type_operable(const type_spec& type) const;
    bool is_modifiable() const;
    bool is_incomplete_type() const;
    bool is_const() const; 
    bool operator == (const type_spec& type) const;

    bool is_type_convertible(type_spec& type);
    bool is_convertible_to_pointer_type();
    static int convert_type(const type_spec& dest_type, int src_id, type_spec& src_type, Ifunc_translation* fn_intf, bool do_phy_conv = true);

    std::pair<c_type, bool> get_simple_type() const;
};
