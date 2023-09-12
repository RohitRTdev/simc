#include <array>
#include "lib/code-gen.h"
#include "lib/x64/x64.h"

const std::vector<std::array<std::string, NUM_REGS>> regs = {{"rax", "rbx", "rcx", "rdx", "rsi", "rdi"},
                                                            {"eax", "ebx", "ecx", "edx", "esi", "edi"},
                                                            {"ax", "bx", "cx", "dx", "si", "di"},
                                                            {"al", "bl", "cl", "dl", "sil", "dil"}};


int x64_func::new_label_id = 0;

x64_func::x64_func(std::string_view name, x64_tu* _parent, c_type ret_type, bool is_signed): new_id(1), cur_offset(0), 
fn_name(name), parent(_parent), ret_label_id(0), m_is_signed(is_signed), m_ret_type(ret_type) {
    //0 indicates reg is free
    for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
        reg_status_list[reg_idx] = 0;
        reg_no_clobber_list[reg_idx] = false;
    }
}

int x64_func::assign_var(int var_id, int id) {
    auto opt = fetch_var(var_id); 
    CRITICAL_ASSERT(opt, "assign_var() called with invalid variable id:{}", var_id);
    auto& var = *opt;
    int offset = var.offset;
    int reg = fetch_result(id);

    CRITICAL_ASSERT(reg_type_list[reg] == var.var_info.type,
    "var type of var_id:{} != expr_id:{} type", var_id, id);

    auto type = reg_type_list[reg];

    insert_code("mov{} %{}, {}(%rbp)", type, reg, std::to_string(offset));
    return reg_status_list[reg];
}

int x64_func::assign_var(int var_id, std::string_view constant) {
    auto opt = fetch_var(var_id); 
    CRITICAL_ASSERT(opt, "assign_var() called with invalid variable id:{}", var_id);
    auto& var = *opt; 
    auto type = var.var_info.type;
    int offset = var.offset;
    
    insert_code("mov{} ${}, {}(%rbp)", type, constant, std::to_string(offset));
    return var_id;
}

int x64_func::assign_to_mem(int id1, int id2) {
    int reg1 = fetch_result(id1);
    reg_no_clobber_list[reg1] = true;
    int reg2 = fetch_result(id2);

    auto type = reg_type_list[reg2];

    CRITICAL_ASSERT(reg_type_list[reg1] == C_LONG,
    "assign_to_mem called with expr_id:{} not of pointer type", id1);

    add_inst_to_code(INSTRUCTION("mov{} %{}, (%{})", inst_suffix[type], 
    get_register_string(type, reg2), get_register_string(C_LONG, reg1))); 

    free_reg(reg1);
    return reg_status_list[reg2];
}

int x64_func::assign_to_mem(int id, std::string_view constant, c_type type) {
    int reg = fetch_result(id);

    filter_type(type);

    CRITICAL_ASSERT(reg_type_list[reg] == C_LONG,
    "assign_to_mem called with expr_id:{} not of pointer type", id);

    add_inst_to_code(INSTRUCTION("mov{} ${}, (%{})", inst_suffix[type], constant, get_register_string(C_LONG, reg)));
    free_reg(reg);
    return id;
}

int x64_func::fetch_from_mem(int id, c_type type, bool is_signed) {
    auto [reg, _, _type] = unary_op_fetch(id);
    filter_type(type, is_signed);
    insert_code("mov{} (%{}), %{}", type, get_register_string(C_LONG, reg), reg);

    set_reg_type(reg, type, is_signed);
    return reg_status_list[reg];
}

int x64_func::fetch_from_mem(std::string_view constant, c_type type, bool is_signed) {
    filter_type(type, is_signed);
    int reg = choose_free_reg(type, is_signed);
    insert_code("mov{} ${}, %{}", type, constant, reg);
    insert_code("mov{} (%{}), %{}", type, get_register_string(C_LONG, reg), reg);

    return reg_status_list[reg];
}

int x64_func::fetch_global_var(int id) {
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    int reg = choose_free_reg(type, var.is_signed);
    insert_code("mov{} {}(%rip), %{}", type, var.name, reg);

    return reg_status_list[reg];
}

int x64_func::assign_global_var(int id, int expr_id) {
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    int reg = fetch_result(expr_id);
    insert_code("mov{} %{}, {}(%rip)", type, reg, var.name);

    return reg_status_list[reg];
}

int x64_func::assign_global_var(int id, std::string_view constant) {
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    add_inst_to_code(INSTRUCTION("mov{} ${}, {}(%rip)", inst_suffix[type], 
    constant, var.name));

    return id;
}


int x64_func::get_address_of(std::string_view constant) {
    int reg = choose_free_reg(C_LONG, false);
    insert_code("lea{} {}(%rip), %{}", C_LONG, constant, reg);

    return reg_status_list[reg];
}

int x64_func::get_address_of(int id, bool is_mem, bool is_global) {
    auto pos = fetch_var(id);
    if(is_global) {
        auto& var = parent->fetch_global_variable(id);
        return get_address_of(var.name);
    }
    int reg = 0;
    if(pos && !is_mem) {
        reg = choose_free_reg(C_LONG, false);
        insert_code("lea{} {}(%rbp), %{}", C_LONG, std::to_string(pos->offset), reg);
    }
    else {
        reg = fetch_result(id);
        insert_code("lea{} (%{}), %{}", C_LONG, reg, reg);
        set_reg_type(reg, C_LONG, false);
    }

    return reg_status_list[reg];
}

int x64_func::declare_local_variable(std::string_view name, c_type type, bool is_signed) {
    filter_type(type, is_signed);
    c_expr_x64 var{};

    c_var var_info{};
    var_info.name = name;
    var_info.type = type; 
    var_info.is_signed = is_signed;
    var.var_info = var_info;
    var.is_local = true;
    var.offset = advance_offset(type);
    var.is_var = true;
    var.id = new_id++;
    
    sim_log_debug("Declaring variable {} of type {} at offset:{} with var_id:{}", name, type, cur_offset, var.id);
    
    id_list.push_back(var);

    return var.id;
}

int x64_func::declare_local_mem_variable(std::string_view name, size_t mem_var_size) {
    c_expr_x64 var{};

    c_var var_info{};
    var_info.name = name;
    var_info.mem_var_size = mem_var_size;
    var.is_local = true;
    var.is_var = true;
    var.offset = advance_offset(mem_var_size);
    var.id = new_id++;
    
    sim_log_debug("Declaring mem variable {} at offset:{} with var_id:{}", name, cur_offset, var.id);

    id_list.push_back(var);

    return var.id;
}

void x64_func::free_result(int exp_id) {
    sim_log_debug("Freeing expr_id:{}", exp_id);
    auto pos = std::find(reg_status_list.begin(),reg_status_list.end(), exp_id);
    if(pos != reg_status_list.end()) {
        int reg_idx = pos - reg_status_list.begin();
        free_reg(reg_idx);
        return;
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
    code = std::string(fn_name) + ":\n" + frame; 
}
