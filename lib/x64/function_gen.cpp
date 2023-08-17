#include <array>
#include "lib/code-gen.h"
#include "lib/x64/x64.h"

const std::vector<std::array<std::string, NUM_REGS>> regs = {{"rax", "rbx", "rcx", "rdx", "rsi", "rdi"},
                                                            {"eax", "ebx", "ecx", "edx", "esi", "edi"},
                                                            {"ax", "bx", "cx", "dx", "si", "di"},
                                                            {"al", "bl", "cl", "dl", "sil", "dil"}};


int x64_func::new_label_id = 0;

x64_func::x64_func(const std::string& name, x64_tu* _parent, size_t ids): new_id(1), cur_offset(0), 
fn_name(name), parent(_parent), threshold_id(ids), ret_label_id(0) {
    //0 indicates reg is free
    for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
        reg_status_list[reg_idx] = 0;
        reg_no_clobber_list[reg_idx] = false;
    }
        
}

int x64_func::assign_var(int var_id, int id) {
    auto& var = fetch_var(var_id);
    int offset = var.offset;
    int reg = fetch_result(id);

    CRITICAL_ASSERT(reg_type_list[reg] == var.var_info.value().type,
    "var type of var_id:{} != expr_id:{} type", var_id, id);

    auto type = reg_type_list[reg];

    add_inst_to_code(INSTRUCTION("mov{} %{}, {}(%rbp)", inst_suffix[type], get_register_string(type, reg), offset));

    return id;
}

int x64_func::assign_var(int var_id, std::string_view constant) {
    
    auto& var = fetch_var(var_id);
    auto type = var.var_info.value().type;
    int offset = var.offset;
    add_inst_to_code(INSTRUCTION("mov{} ${}, {}(%rbp)", inst_suffix[type], constant, offset));

    return var_id;
}

int x64_func::assign_to_mem(int id1, int id2) {
    int reg1 = fetch_result(id1);
    reg_no_clobber_list[reg1] = true;
    int reg2 = fetch_result(id2);

    auto type = reg_type_list[reg2];

    CRITICAL_ASSERT(reg_type_list[reg1] != C_LONG,
    "assign_to_mem called with expr_id:{} not of pointer type", id1);

    add_inst_to_code(INSTRUCTION("mov{} %{}, (%{})", inst_suffix[type], 
    get_register_string(type, reg2), regs[0][reg1])); 
    reg_no_clobber_list[reg1] = false;

    return id2;
}

int x64_func::assign_to_mem(int id, std::string_view constant, c_type type) {
    int reg = fetch_result(id);

    CRITICAL_ASSERT(reg_type_list[reg] != C_LONG,
    "assign_to_mem called with expr_id:{} not of pointer type", id);

    add_inst_to_code(INSTRUCTION("mov{} ${}, (%{})", inst_suffix[type], constant, regs[0][reg])); 

    return id;
}

int x64_func::fetch_global_var(int id) {
    CRITICAL_ASSERT(id <= threshold_id, "Global var id:{} exceeds threshold:{}", id, threshold_id);

    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    int reg = choose_free_reg(type, var.is_signed);
    add_inst_to_code(INSTRUCTION("mov{} {}(%rip), %{}", inst_suffix[type], 
    var.name, get_register_string(type, reg)));

    return reg_status_list[reg];
}

int x64_func::assign_global_var(int id, int expr_id) {
    CRITICAL_ASSERT(id <= threshold_id, "Global var id:{} exceeds threshold:{}", id, threshold_id);
    
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    int reg = fetch_result(expr_id);
    add_inst_to_code(INSTRUCTION("mov{} %{}, {}(%rip)", inst_suffix[type], 
    get_register_string(type, reg), var.name));

    return reg_status_list[reg];
}

int x64_func::assign_global_var(int id, std::string_view constant) {
    CRITICAL_ASSERT(id <= threshold_id, "Global var id:{} exceeds threshold:{}", id, threshold_id);

    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    add_inst_to_code(INSTRUCTION("mov{} ${}, {}(%rip)", inst_suffix[type], 
    constant, var.name));

    return id;
}

int x64_func::declare_local_variable(const std::string& name, c_type type, bool is_signed) {
    c_expr_x64 var;

    sim_log_debug("Declaring variable {} of type {} at offset:{} with var_id:{}", name, type, cur_offset, new_id);
    var.var_info = {name, type, is_signed};
    var.is_local = true;
    var.offset = advance_offset(type);
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
        if(loc.id == exp_id && !loc.is_var) {
            if(!loc.cached) {
                sim_log_debug("exp_id:{} already freed up in stack location", exp_id);
            }
            loc.cached = false;
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
