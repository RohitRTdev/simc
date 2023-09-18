#include "lib/x64/x64.h"

int x64_func::add(int id1, int id2) {
    auto [reg1, reg2, type] = binary_op_fetch(id1, id2);

    insert_code("add{} %{}, %{}", type, reg2, reg1);

    free_reg(reg2);
    return reg_status_list[reg1];
}

int x64_func::add(int id, std::string_view constant) {
    auto [reg1, _, type] = unary_op_fetch(id);

    insert_code("add{} ${}, %{}", type, constant, reg1);

    return reg_status_list[reg1];
}

int x64_func::sub(int id1, int id2) {

    auto [reg1, reg2, type] = binary_op_fetch(id1, id2);

    insert_code("sub{} %{}, %{}", type, reg2, reg1);

    free_reg(reg2);
    return reg_status_list[reg1];
}

int x64_func::sub(int id, std::string_view constant) {
    auto [reg1, _, type] = unary_op_fetch(id);

    insert_code("sub{} ${}, %{}", type, constant, reg1);

    return reg_status_list[reg1];
}

int x64_func::sub(std::string_view constant, int id) {
    auto [reg1, _, type] = unary_op_fetch(id);
    reg_no_clobber_list[reg1] = true;
    int reg = choose_free_reg(type, sign_list[reg1]);
    insert_code("mov{} ${}, %{}", type, constant, reg);
    insert_code("sub{} %{}, %{}", type, reg1, reg);

    free_reg(reg1);
    return reg_status_list[reg];
}

int x64_func::mul(int id1, int id2) {

    auto [reg1, reg2, type] = binary_op_fetch(id1, id2);

    bool is_signed = sign_list[reg1];
    int reg = reg1 != RAX ? reg1 : reg2;
    if (reg1 != RAX && reg2 != RAX) {
        transfer_to_reg(RAX, reg_status_list[reg1]);
    }
        
    if(is_signed) {
        insert_code("imul{} %{}, %{}", type, reg, RAX);
    }
    else {
        insert_code("mul{} %{}", type, reg);
    }

    free_reg(reg);
    return reg_status_list[RAX];
}

int x64_func::mul(int id1, std::string_view constant) {
    auto [type, is_signed] = fetch_result_type(id1);
    free_preferred_register(RAX, type, is_signed);
    reg_no_clobber_list[RAX] = true;
    int reg1 = fetch_result(id1);
    insert_code("mov{} ${}, %{}", type, constant, RAX);
    if(is_signed) {
        insert_code("imul{} %{}, %{}", type, reg1, RAX);
    }
    else {
        insert_code("mul{} %{}", type, reg1);
    }

    free_reg(reg1);
    reg_no_clobber_list[RAX] = false;
    return reg_status_list[RAX];
}


int x64_func::div_common(int id1, std::variant<int, std::string_view> object, bool is_div, bool in_order) {
    auto [type, is_signed] = fetch_result_type(id1);
    free_preferred_register(RDX, type, is_signed);
    reg_no_clobber_list[RDX] = true;
    

    auto [reg1, reg2] = arg_fetch(id1, type, is_signed, object, in_order, [&] (int reg1) {
        if(reg1 != RAX) {
            transfer_to_reg(RAX, reg_status_list[reg1]);
        }
        reg_no_clobber_list[RAX] = true;
    });

    insert_code("mov{} $0, %{}", type, RDX);
    if(is_signed) {
        insert_code("idiv{} %{}", type, reg2);
    }
    else {
        insert_code("div{} %{}", type, reg2);
    }

    free_reg(reg2);
    reg_no_clobber_list[RDX] = reg_no_clobber_list[RAX] = false;
    if(is_div) {
        //Quotient stored in RAX
        free_reg(RDX);
        return reg_status_list[RAX];
    }
    else {
        //Remainder stored in RDX
        free_reg(RAX);
        return reg_status_list[RDX];
    }
}

int x64_func::div(int id1, int id2) {
    return div_common(id1, id2, true);
}

int x64_func::div(int id1, std::string_view constant) {
    return div_common(id1, constant, true);
}

int x64_func::div(std::string_view constant, int id) {
    return div_common(id, constant, true, false);
}

int x64_func::modulo(int id1, int id2) {
    return div_common(id1, id2, false);
}

int x64_func::modulo(int id1, std::string_view constant) {
    return div_common(id1, constant, false);
}

int x64_func::modulo(std::string_view constant, int id) {
    return div_common(id, constant, false, false);
}

int x64_func::and_or_xor_common(int id1, std::variant<int, std::string_view> object, bool is_and, bool is_or) {
    auto [type, is_signed] = fetch_result_type(id1);
    
    auto [reg1, reg2] = arg_fetch(id1, type, is_signed, object, true);
    
    std::string inst = is_and ? "and" : is_or ? "or" : "xor";
    insert_code(inst + "{} %{}, %{}", type, reg2, reg1);
    free_reg(reg2);
    reg_no_clobber_list[reg1] = false;
    
    return reg_status_list[reg1];
}

int x64_func::bit_and(int id1, int id2) {
    return and_or_xor_common(id1, id2, true, false);
}

int x64_func::bit_and(int id1, std::string_view constant) {
    return and_or_xor_common(id1, constant, true, false);
}

int x64_func::bit_or(int id1, int id2) {
    return and_or_xor_common(id1, id2, false, true);
}

int x64_func::bit_or(int id1, std::string_view constant) {
    return and_or_xor_common(id1, constant, false, true);
}

int x64_func::bit_xor(int id1, int id2) {
    return and_or_xor_common(id1, id2, false, false);
}

int x64_func::bit_xor(int id1, std::string_view constant) {
    return and_or_xor_common(id1, constant, false, false);
}

int x64_func::bit_not(int id) {
    auto [reg, _, type] = unary_op_fetch(id);
    insert_code("not{} %{}", type, reg);

    return reg_status_list[reg];
}

int x64_func::type_cast(int exp_id, c_type cast_type, bool cast_sign) {
    auto [reg1, _, type] = unary_op_fetch(exp_id);
    bool is_signed = sign_list[reg1];

    filter_type(cast_type, cast_sign);
    if(is_type_lower(cast_type, type)) {
        set_reg_type(reg1, cast_type, cast_sign);
        return reg_status_list[reg1];
    }
    
    char src_suffix = inst_suffix[type];
    if(src_suffix != 'l' && src_suffix != 'q') {
        set_reg_type(reg1, cast_type, cast_sign);
        add_inst_to_code(INSTRUCTION("mov{}{}{} %{}, %{}", is_signed ? "s" : "z", 
        src_suffix, inst_suffix[cast_type],
        get_register_string(type, reg1), get_register_string(cast_type, reg1)));
    }
    else {
        if (is_signed) {
            transfer_to_reg(RAX, reg_status_list[reg1]);
            add_inst_to_code(INSTRUCTION("cltq"));
            reg1 = RAX;
        }
        set_reg_type(RAX, cast_type, cast_sign);
    }

    return reg_status_list[reg1];
}


//Common function to handle pre/post/inc/dec cases
int x64_func::inc_common(int id, c_type type, bool is_signed, size_t _inc_count, bool is_pre, bool inc, bool is_mem, bool is_global) {
    filter_type(type, is_signed);
    auto pos = fetch_var(id, true);
    std::string inst;
    std::string inc_count = std::to_string(_inc_count);
    inst = inc ? "add" : "sub";
    //Check if it is a variable(Increment by one if it is)

    if (is_global) {
        auto& var = parent->fetch_global_variable(id);
        inst += "{} ${}, {}(%rip)";

        if (is_pre) {
            insert_code(inst, var.type, inc_count, var.name); 
            return fetch_global_var(id);
        }
        else {
            int new_id = fetch_global_var(id);
            insert_code(inst, var.type, inc_count, var.name);

            return new_id;
        }
    }
    else if(pos && !is_mem) {
        inst += "{} ${}, {}(%rbp)"; 
        if(is_pre) {
            insert_code(inst, pos->var_info.type, inc_count, std::to_string(pos->offset));
            return reg_status_list[load_var(*pos)];
        }
        else {
            int reg = load_var(*pos);
            insert_code(inst, pos->var_info.type, inc_count, std::to_string(pos->offset));
            return reg_status_list[reg];
        }
    }
    //Otherwise it is a computed lvalue expression(such as an array member or dereferenced expression)
    else {
        auto [reg, _, __] = unary_op_fetch(id);
        inst += "{} ${}, (%{})";
        if(is_pre) {
            insert_code(inst, type, inc_count, get_register_string(C_LONG, reg));
            insert_code("mov{} (%{}), %{}", type, get_register_string(C_LONG, reg), reg);
            set_reg_type(reg, type, is_signed);
            return reg_status_list[reg];
        }
        else {
            int reg_hold = choose_free_reg(type, is_signed);
            insert_code("mov{} (%{}), %{}", type, get_register_string(C_LONG, reg), reg_hold);
            insert_code(inst, type, inc_count, get_register_string(C_LONG, reg));
            free_reg(reg);

            return reg_status_list[reg_hold];
        }
    }

    CRITICAL_ASSERT_NOW("Invalid id passed to inc_common()!")
}

int x64_func::pre_inc(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) {
    return inc_common(id, type, is_signed, inc_count, true, true, is_mem, is_global);
}

int x64_func::pre_dec(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) {
    return inc_common(id, type, is_signed, inc_count, true, false, is_mem, is_global);
}

int x64_func::post_inc(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) {
    return inc_common(id, type, is_signed, inc_count, false, true, is_mem, is_global);
}

int x64_func::post_dec(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) {
    return inc_common(id, type, is_signed, inc_count, false, false, is_mem, is_global);
}

int x64_func::negate(int id) {
    auto [reg, _, type] = unary_op_fetch(id);

    insert_code("neg{} %{}", type, reg);
    return reg_status_list[reg];
}