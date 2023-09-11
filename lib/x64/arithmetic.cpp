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

int x64_func::type_cast(int exp_id, c_type cast_type, bool cast_sign) {
    auto [reg1, _, type] = unary_op_fetch(exp_id);
    bool is_signed = sign_list[reg1];

    filter_type(cast_type, cast_sign);
    set_reg_type(reg1, cast_type, cast_sign);
    if(cast_type <= type) {
        return reg_status_list[reg1];
    }
   
    add_inst_to_code(INSTRUCTION("mov{}{}{} %{}, %{}", is_signed ? "s" : "z", 
    inst_suffix[type], inst_suffix[cast_type],
    get_register_string(type, reg1), get_register_string(cast_type, reg1)));

    return reg_status_list[reg1];
}

