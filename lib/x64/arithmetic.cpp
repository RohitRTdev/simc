#include "lib/x64/x64.h"

int x64_func::add_int_c(int id, std::string_view constant) {
    int reg = fetch_result(id);
    add_inst_to_code(INSTRUCTION("addl ${}, %{}", constant, regs_32[reg]));

    return reg_status_list[reg];
}

int x64_func::add_int(int id1, int id2) {
    int reg1 = fetch_result(id1);
    reg_no_clobber_list[reg1] = true;
    
    int reg2 = fetch_result(id2);
    reg_no_clobber_list[reg2] = true;

//When added to no_clobber list that register will not be freed up and used in case no registers are available
    int reg3 = choose_free_reg();

    add_inst_to_code(INSTRUCTION("leal (%{}, %{}), %{}", regs_64[reg1], regs_64[reg2], regs_32[reg3]));

//Clear them as they can now be freed up
    reg_no_clobber_list[reg1] = reg_no_clobber_list[reg2] = false;

    return reg_status_list[reg3];
}


int x64_func::sub_int(int id1, int id2) {
    int reg1 = fetch_result(id1);
    reg_no_clobber_list[reg1] = true;
    
    int reg2 = fetch_result(id2);
    reg_no_clobber_list[reg2] = true;

    int reg3 = choose_free_reg();
    
    add_inst_to_code(INSTRUCTION("movl %{}, %{}", regs_32[reg1], regs_32[reg3]));
    add_inst_to_code(INSTRUCTION("subl %{}, %{}", regs_32[reg2], regs_32[reg3]));

    reg_no_clobber_list[reg1] = reg_no_clobber_list[reg2] = false;

    return reg_status_list[reg3];
}

int x64_func::sub_int_c(int id, std::string_view constant) {
    int reg = fetch_result(id);

    add_inst_to_code(INSTRUCTION("subl ${}, %{}", constant, regs_32[reg]));

    return reg_status_list[reg];
}

int x64_func::sub_int_c(std::string_view constant, int id) {
    int reg = fetch_result(id);
    reg_no_clobber_list[reg] = true;
    int reg1 = choose_free_reg();

    add_inst_to_code(INSTRUCTION("movl ${}, %{}", constant, regs_32[reg1]));
    add_inst_to_code(INSTRUCTION("subl %{}, %{}", regs_32[reg], regs_32[reg1]));

    reg_no_clobber_list[reg] = false;

    return reg_status_list[reg1];
}


