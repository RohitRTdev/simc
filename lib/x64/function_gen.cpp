#include "lib/code-gen.h"
#include "lib/x64/x64.h"

const char* regs_64[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi"};
const char* regs_32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi"};

x64_func::x64_func(): new_id(1), cur_offset(0), var_id(0) {
    //0 indicates reg is free
    for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
        reg_status_list[reg_idx] = 0;
        reg_no_clobber_list[reg_idx] = false;
    }
        
}

void x64_func::assign_ii(int var_id1, int var_id2) {
    int reg = load_var_32(var_id2);
    int offset = vars[var_id1].loc.offset;

    add_inst_to_code(INSTRUCTION("movl %{}, {}(%rbp)", regs_32[reg], offset));
    free_reg(reg);
}

void x64_func::assign_ic(int var_id, int constant) {
    
    int offset = vars[var_id].loc.offset;
    add_inst_to_code(INSTRUCTION("movl ${}, {}(%rbp)", constant, offset));
}

void x64_func::assign_ir(int var_id, int exp_id) {
    int offset = vars[var_id].loc.offset;
    int reg = fetch_result(exp_id);

    add_inst_to_code(INSTRUCTION("movl %{}, {}(%rbp)", regs_32[reg], offset));
    free_reg(reg);
}

int x64_func::declare_local_variable(const std::string& name, c_type type) {
    c_var_x64 var;

    sim_log_debug("Declaring variable {} of type {} at offset:{} with var_id:{}", name, type, cur_offset, var_id);
    var.var_info.name = name;
    var.var_info.type = type;
    var.is_local = true;
    var.loc.offset = advance_offset_32();

    vars.push_back(var);

    return var_id++;
}

Ifunc_translation* create_function_translation_unit() {
    auto* unit = new x64_func();
    return unit;
}
