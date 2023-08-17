#include "lib/x64/x64.h"

int x64_func::add(int id1, int id2) {
    auto [reg1, reg2, reg, type] = binary_op_fetch(id1, id2);

    add_inst_to_code(INSTRUCTION("lea{} (%{}, %{}), %{}", inst_suffix[type], 
    get_register_string(type, reg1), get_register_string(type, reg2), get_register_string(type, reg)));


    reg_no_clobber_list[reg1] = reg_no_clobber_list[reg2] = false;
    free_reg(reg1), free_reg(reg2);

    return reg_status_list[reg];
}

int x64_func::add(int id, std::string_view constant) {
    auto [reg1, _, reg, type] = unary_op_fetch(id);

    add_inst_to_code(INSTRUCTION("lea{} {}(%{}), %{}", inst_suffix[type], 
    constant, get_register_string(type, reg1), get_register_string(type, reg)));

    reg_no_clobber_list[reg1] = false;
    free_reg(reg1);

    return reg_status_list[reg];
}

int x64_func::sub(int id1, int id2) {

    auto [reg1, reg2, reg, type] = binary_op_fetch(id1, id2);

    add_inst_to_code(INSTRUCTION("mov{} %{}, %{}", inst_suffix[type],
    get_register_string(type, reg1), get_register_string(type, reg)));
    add_inst_to_code(INSTRUCTION("subl %{}, %{}", inst_suffix[type],
    get_register_string(type, reg2), get_register_string(type, reg)));

    reg_no_clobber_list[reg1] = reg_no_clobber_list[reg2] = false;

    free_reg(reg1), free_reg(reg2);
    return reg_status_list[reg];
}

int x64_func::sub(int id, std::string_view constant) {
    auto [reg1, _, reg, type] = unary_op_fetch(id);

    add_inst_to_code(INSTRUCTION("mov{} %{}, %{}", inst_suffix[type], 
    get_register_string(type, reg1), get_register_string(type, reg)));
    add_inst_to_code(INSTRUCTION("sub{} ${}, %{}", inst_suffix[type], constant,
    get_register_string(type, reg)));

    reg_no_clobber_list[reg1] = false;
    free_reg(reg1);

    return reg_status_list[reg];
}

int x64_func::sub(std::string_view constant, int id) {
    auto [reg1, _, reg, type] = unary_op_fetch(id);

    add_inst_to_code(INSTRUCTION("mov{} ${}, %{}", inst_suffix[type], constant,
    get_register_string(type, reg)));
    add_inst_to_code(INSTRUCTION("sub{} %{}, %{}", inst_suffix[type], 
    get_register_string(type, reg1), get_register_string(type, reg)));

    reg_no_clobber_list[reg1] = false;
    free_reg(reg1);

    return reg_status_list[reg];
}
