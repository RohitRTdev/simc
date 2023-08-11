#include "lib/code-gen.h"
#include "lib/x64/x64.h"

std::vector<std::string> regs_64 = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi"};
std::vector<std::string> regs_32 = {"eax", "ebx", "ecx", "edx", "esi", "edi"};

int x64_func::new_label_id = 0;

x64_func::x64_func(const std::string& name, x64_tu* _parent, size_t ids): new_id(1), cur_offset(0), 
fn_name(name), parent(_parent), threshold_id(ids), ret_label_id(0) {
    //0 indicates reg is free
    for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
        reg_status_list[reg_idx] = 0;
        reg_no_clobber_list[reg_idx] = false;
    }
        
}

void x64_func::assign_var_int(int var_id, int id) {
    int offset = fetch_var(var_id).loc.offset;
    int reg = fetch_result(id);

    add_inst_to_code(INSTRUCTION("movl %{}, {}(%rbp)", regs_32[reg], offset));
}

void x64_func::assign_var_int_c(int var_id, std::string_view constant) {
    
    int offset = fetch_var(var_id).loc.offset;
    add_inst_to_code(INSTRUCTION("movl ${}, {}(%rbp)", constant, offset));
}

void x64_func::assign_to_mem_int(int id1, int id2) {
    int reg1 = fetch_result(id1);
    reg_no_clobber_list[reg1] = true;
    int reg2 = fetch_result(id2);

    add_inst_to_code(INSTRUCTION("movl %{}, (%{})", regs_32[reg2], regs_64[reg1])); 
    reg_no_clobber_list[reg1] = false;

}

void x64_func::assign_to_mem_int_c(int id, std::string_view constant) {
    int reg = fetch_result(id);

    add_inst_to_code(INSTRUCTION("movl ${}, (%{})", constant, regs_64[reg])); 
}

int x64_func::fetch_global_var_int(int id) {
    CRITICAL_ASSERT(id <= threshold_id, "Global var id:{} exceeds threshold:{}", id, threshold_id);

    int reg = choose_free_reg();
    add_inst_to_code(INSTRUCTION("movl {}(%rip), %{}", parent->fetch_global_variable(id), regs_32[reg]));

    return reg_status_list[reg];
}

void x64_func::assign_global_var_int(int id, int expr_id) {
    CRITICAL_ASSERT(id <= threshold_id, "Global var id:{} exceeds threshold:{}", id, threshold_id);
    
    int reg = fetch_result(expr_id);
    add_inst_to_code(INSTRUCTION("movl %{}, {}(%rip)", regs_32[reg], parent->fetch_global_variable(id)));
}

void x64_func::assign_global_var_int_c(int id, std::string_view constant) {
    CRITICAL_ASSERT(id <= threshold_id, "Global var id:{} exceeds threshold:{}", id, threshold_id);

    add_inst_to_code(INSTRUCTION("movl ${}, {}(%rip)", constant, parent->fetch_global_variable(id)));
}

int x64_func::declare_local_variable(const std::string& name, c_type type) {
    c_expr_x64 var;

    sim_log_debug("Declaring variable {} of type {} at offset:{} with var_id:{}", name, type, cur_offset, new_id);
    var.var_info = {name, type};
    var.is_local = true;
    var.loc.offset = advance_offset_32();
    var.is_var = true;
    var.id = new_id++;
    
    id_list.push_back(var);

    return var.id;
}

void x64_func::free_result(int exp_id) {
    sim_log_debug("Freeing expr_id:{}", exp_id);
    for(auto& reg: reg_status_list) {
        if(reg == exp_id) {
            reg = 0;
            break;
        }
    }

    for(auto& loc: id_list) {
        if(loc.id == exp_id) {
            if(!loc.cached) {
                sim_log_debug("exp_id:{} already freed up in stack location", exp_id);
            }
            loc.is_var = loc.cached = false;
            break;
        }
    }
}

void x64_func::generate_code() {
    std::string frame1 = INSTRUCTION("pushq %rbp"); 
    std::string frame2 = INSTRUCTION("movq %rsp, %rbp");

    cur_offset = -ALIGN(cur_offset, 16);
    std::string frame3;
    if(cur_offset)
        frame3 = INSTRUCTION("subq ${}, %rsp", -cur_offset); 

    std::string endframe = INSTRUCTION("leave\n\tret");

    if(ret_label_id)
        endframe = fmt::format(LINE(".L{}:"), ret_label_id) + endframe;

    std::string frame;
    if(code.size())
        frame = frame1 + frame2 + frame3 + code + endframe;
    else
        frame = "\tnop\n\tret";
    code = fn_name + ":\n" + frame; 
}
