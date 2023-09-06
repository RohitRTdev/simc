#include "lib/x64/x64.h"

int x64_func::add(int id1, int id2) {
    auto [reg1, reg2, reg, type] = binary_op_fetch(id1, id2);

    insert_code("lea{} (%{}, %{}), %{}", type, reg1, reg2, reg);

    free_reg(reg1), free_reg(reg2);
    return reg_status_list[reg];
}

int x64_func::add(int id, std::string_view constant) {
    auto [reg1, _, reg, type] = unary_op_fetch(id);

    insert_code("lea{} {}(%{}), %{}", type, constant, reg1, reg);

    free_reg(reg1);
    return reg_status_list[reg];
}

int x64_func::sub(int id1, int id2) {

    auto [reg1, reg2, reg, type] = binary_op_fetch(id1, id2);

    insert_code("mov{} %{}, %{}", type, reg1, reg);
    insert_code("sub{} %{}, %{}", type, reg2, reg);

    free_reg(reg1), free_reg(reg2);
    return reg_status_list[reg];
}

int x64_func::sub(int id, std::string_view constant) {
    auto [reg1, _, reg, type] = unary_op_fetch(id);

    insert_code("mov{} %{}, %{}", type, reg1, reg);
    insert_code("sub{} ${}, %{}", type, constant, reg);

    free_reg(reg1);
    return reg_status_list[reg];
}

int x64_func::sub(std::string_view constant, int id) {
    auto [reg1, _, reg, type] = unary_op_fetch(id);

    insert_code("mov{} ${}, %{}", type, constant, reg);
    insert_code("sub{} ${}, %{}", type, reg1, reg);

    free_reg(reg1);
    return reg_status_list[reg];
}

int x64_func::mul(int id1, int id2) {
    auto [type, is_signed] = fetch_result_type(id1);
    free_preferred_register(RAX, type, is_signed);
    reg_no_clobber_list[RAX] = true;

    int reg1 = fetch_result(id1);
    reg_no_clobber_list[reg1] = true;
    int reg2 = fetch_result(id2);

    insert_code("mov{} %{}, %{}", type, reg1, RAX);
    if(is_signed) {
        insert_code("imul{} %{}, %{}", type, reg2, RAX);
    }
    else {
        insert_code("mul{} %{}", type, reg2);
    }

    free_reg(reg1), free_reg(reg2);
    reg_no_clobber_list[RAX] = false;
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