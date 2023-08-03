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

    add_inst_to_code(INSTRUCTION("movl %{}, %{}", regs_32[reg1], regs_32[reg3]));
    add_inst_to_code(INSTRUCTION("addl %{}, %{}", regs_32[reg2], regs_32[reg3]));

//Clear them as they can now be freed up
    reg_no_clobber_list[reg1] = reg_no_clobber_list[reg2] = false;

    return reg_status_list[reg3];
}


