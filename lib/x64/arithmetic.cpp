#include "lib/x64/x64.h"

int x64_func::add_ii(int var_id1, int var_id2) {
    int reg = load_var_32(var_id1);
    int offset = vars[var_id2].loc.offset;
    add_inst_to_code(INSTRUCTION("addl {}(%rbp), %{}", offset, regs_32[reg]));
    
    return reg_status_list[reg];
}

int x64_func::add_ic(int var_id1, int constant) {
    int reg = load_var_32(var_id1);
    add_inst_to_code(INSTRUCTION("addl ${}, %{}", constant, regs_32[reg]));
    
    return reg_status_list[reg];
}

int x64_func::add_ri(int exp_id, int var_id) {

    //We need to see if the result is saved in stack or in a reg
    int reg = fetch_result(exp_id);

    add_inst_to_code(INSTRUCTION("addl {}(%rbp), %{}", vars[var_id].loc.offset, regs_32[reg]));
   
    return reg_status_list[reg];
}

int x64_func::add_rc(int exp_id, int constant) {
    int reg = fetch_result(exp_id);
    add_inst_to_code(INSTRUCTION("addl ${}, %{}", constant, regs_32[reg]));

    return reg_status_list[reg];
}

int x64_func::add_rr(int exp_id1, int exp_id2) {
    int reg1 = fetch_result(exp_id1);
    reg_no_clobber_list[reg1] = true;
    
    int reg2 = fetch_result(exp_id2);
    reg_no_clobber_list[reg2] = true;

//When added to no_clobber list that register will not be freed up and used in case no registers are available
    int reg3 = choose_free_reg();

    add_inst_to_code(INSTRUCTION("movl %{}, %{}", regs_32[reg1], regs_32[reg3]));
    add_inst_to_code(INSTRUCTION("addl %{}, %{}", regs_32[reg2], regs_32[reg3]));

//Clear them as they can now be freed up
    reg_no_clobber_list[reg1] = reg_no_clobber_list[reg2] = false;

    return reg_status_list[reg3];

}

void x64_func::generate_code() {
    std::string frame1 = INSTRUCTION("pushq %rbp"); 
    std::string frame2 = INSTRUCTION("movq %rsp, %rbp");

    cur_offset = -ALIGN(cur_offset, 16);
    std::string frame3;
    if(cur_offset)
        frame3 = INSTRUCTION("subq ${}, %rsp", -cur_offset); 

    std::string endframe = INSTRUCTION("leave\n\tret");

    code = frame1 + frame2 + frame3 + code + endframe; 
}

