#include <array>
#include "lib/code-gen.h"
#include "lib/x64/x64.h"

const std::vector<std::array<std::string, NUM_REGS>> regs = {{"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9"},
                                                            {"eax", "ebx", "ecx", "edx", "esi", "edi", "r8d", "r9d"},
                                                            {"ax", "bx", "cx", "dx", "si", "di", "r8w", "r9w"},
                                                            {"al", "bl", "cl", "dl", "sil", "dil", "r8b", "r9b"}};


int x64_func::new_label_id = 0;

x64_func::x64_func(std::string_view name, x64_tu* _parent, c_type ret_type, bool is_signed): new_id(1), cur_offset(0), 
fn_name(name), parent(_parent), ret_label_id(0), m_is_signed(is_signed), m_ret_type(ret_type), call_index(0),
param_offset(8) {
    //0 indicates reg is free
    int i = 0;
    for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
        bool in_call_list = false;
        reg_status_list[reg_idx] = 0;
        reg_no_clobber_list[reg_idx] = false;
        for(auto reg: reg_call_list) {
            if(reg == reg_idx) {
                in_call_list = true;
                break;
            }
        }
        if(!in_call_list) {
            reg_non_call_list[i++] = reg_idx;
        }
    }
}

int x64_func::assign_var(int var_id, int id) {
    auto opt = fetch_var(var_id); 
    CRITICAL_ASSERT(opt, "assign_var() called with invalid variable id:{}", var_id);
    insert_comment("assign to var");
    
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
    insert_comment("assign constant to var");
    auto& var = *opt; 
    auto type = var.var_info.type;
    int offset = var.offset;
    
    insert_code("mov{} ${}, {}(%rbp)", type, constant, std::to_string(offset));
    return var_id;
}

int x64_func::assign_to_mem(int id1, int id2) {
    insert_comment("assign to mem");
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
    insert_comment("assign constant to mem");
    int reg = fetch_result(id);

    filter_type(type);

    CRITICAL_ASSERT(reg_type_list[reg] == C_LONG,
    "assign_to_mem called with expr_id:{} not of pointer type", id);

    add_inst_to_code(INSTRUCTION("mov{} ${}, (%{})", inst_suffix[type], constant, get_register_string(C_LONG, reg)));
    free_reg(reg);
    return id;
}

int x64_func::fetch_from_mem(int id, c_type type, bool is_signed) {
    insert_comment("fetch from mem");    
    auto [reg, _, _type] = unary_op_fetch(id);
    filter_type(type, is_signed);
    insert_code("mov{} (%{}), %{}", type, get_register_string(C_LONG, reg), reg);

    set_reg_type(reg, type, is_signed);
    return reg_status_list[reg];
}

int x64_func::fetch_from_mem(std::string_view constant, c_type type, bool is_signed) {
    filter_type(type, is_signed);
    insert_comment("fetch from mem constant");    
    int reg = choose_free_reg(type, is_signed);
    insert_code("mov{} ${}, %{}", type, constant, reg);
    insert_code("mov{} (%{}), %{}", type, get_register_string(C_LONG, reg), reg);

    return reg_status_list[reg];
}

int x64_func::fetch_global_var(int id) {
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;

    insert_comment("fetch global var");    
    int reg = choose_free_reg(type, var.is_signed);
    if(var.mem_var_size)
        insert_code("lea{} {}(%rip), %{}", type, var.name, reg);
    else 
        insert_code("mov{} {}(%rip), %{}", type, var.name, reg);

    return reg_status_list[reg];
}

int x64_func::assign_global_var(int id, int expr_id) {
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;
    insert_comment("assign global var");    

    int reg = fetch_result(expr_id);
    insert_code("mov{} %{}, {}(%rip)", type, reg, var.name);

    return reg_status_list[reg];
}

int x64_func::assign_global_var(int id, std::string_view constant) {
    auto& var = parent->fetch_global_variable(id);
    auto type = var.type;
    insert_comment("assign constant to global var");    

    add_inst_to_code(INSTRUCTION("mov{} ${}, {}(%rip)", inst_suffix[type], 
    constant, var.name));

    return id;
}

int x64_func::get_address_of(std::string_view constant) {
    std::string function_name = fmt::format("{}@GOTPCREL", constant);
    int reg = choose_free_reg(C_LONG, false);
    insert_code("mov{} {}(%rip), %{}", C_LONG, function_name, reg);

    return reg_status_list[reg];
}

int x64_func::get_address_of(int id, bool is_mem, bool is_global) {
    auto pos = fetch_var(id);
    insert_comment("get address");
    if(is_global) {
        auto& var = parent->fetch_global_variable(id);
        int reg = choose_free_reg(C_LONG, false);
        insert_code("lea{} {}(%rip), %{}", C_LONG, var.name, reg);
        return reg_status_list[reg];
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
    var_info.type = C_LONG;
    var_info.is_signed = false;
    var.is_local = true;
    var.is_var = true;
    var.offset = advance_offset(mem_var_size);
    var.id = new_id++;
    var.var_info = var_info;
    
    sim_log_debug("Declaring mem variable {} at offset:{} with var_id:{}", name, cur_offset, var.id);

    id_list.push_back(var);

    return var.id;
}

int x64_func::create_temporary_value(c_type type, bool is_signed) {
    int reg = choose_free_reg(type, is_signed);
    return reg_status_list[reg];
}

int x64_func::set_value(int expr_id, std::string_view constant) {
    insert_comment("set value");
    auto [reg, _, type] = unary_op_fetch(expr_id);
    insert_code("mov{} ${}, %{}", type, constant, reg);

    return reg_status_list[reg];
}

int x64_func::save_param(std::string_view name, c_type type, bool is_signed) {
    filter_type(type, is_signed);
    int id;
    if(call_index < reg_call_list.size()) {
        id = declare_local_variable(name, type, is_signed);
        insert_code("mov{} %{}, {}(%rbp)", type, reg_call_list[call_index++], std::to_string(cur_offset));
    }
    else {
        c_expr_x64 var{};
        var.is_local = true;
        var.is_var = true;
        var.offset = advance_param_offset();
        id = var.id = new_id++;
        var.var_info.name = name;
        var.var_info.type = type;
        var.var_info.is_signed = is_signed;
    
        id_list.push_back(var);
    }

    return id;
}

void x64_func::begin_call_frame(size_t num_args) {
    //Save all the registers
    bytes_moved = 0;
    int stack_args = num_args - reg_call_list.size();
    insert_comment("begin call frame");
    
    if(stack_args <= 0) {
        //No need to free up all registers if no stack variables need to be allocated
        //Stack variable allocation requires all registers to be freed since the registers
        //could get saved at a later point on the stack(as temporary value).
        //This would mess up the stack layout if it gets mixed with stack variables being passed as params
        for(int i = 0; i < std::min(num_args, reg_call_list.size()); i++) {
            flush_register(reg_call_list[i]);
        }
    }
    else {
        for(int i = 0; i < reg_status_list.size(); i++) {
            flush_register(i);
        }
    }

    for(int i = 0; i < std::min(num_args, reg_call_list.size()); i++) {
        reg_no_clobber_list[reg_call_list[i]] = true;
    }
    
    if(stack_args > 0) {
        int final_offset = std::abs(cur_offset - stack_args*8);
        if(final_offset % 16) {
            cur_offset -= 8;
            add_inst_to_code(INSTRUCTION("subq $8, %rsp"));
            bytes_moved += 8;
        }
    }

    num_call_args = num_args;
    call_index = 0;

    CRITICAL_ASSERT(param_stack.empty(), "param_stack is not empty");
}

void x64_func::load_param(int exp_id) {
    if(call_index < reg_call_list.size()) 
        transfer_to_reg(reg_call_list[call_index++], exp_id);
    else {
        param_stack.push(exp_id);
    }
}

void x64_func::load_param(std::string_view constant, c_type type, bool is_signed) {
    filter_type(type, is_signed);
    if(call_index < reg_call_list.size()) {
        insert_code("mov{} ${}, %{}", type, constant, reg_call_list[call_index++]);
    }
    else {
        param_stack.push(std::make_tuple(constant, type, is_signed));
    }
}

void x64_func::call_function_begin() {
    while(!param_stack.empty()) {
        push_to_stack(param_stack.top());
        param_stack.pop();
    }
    for(auto reg: reg_non_call_list) {
        flush_register(reg);
    }
}

void x64_func::call_function_end() {
    for(int i = 0; i < std::min(num_call_args, reg_call_list.size()); i++)
        free_reg(reg_call_list[i]);

    if(bytes_moved) {
        add_inst_to_code(INSTRUCTION("addq ${}, %rsp", bytes_moved));
        cur_offset += bytes_moved; //Since stack grows downwards
    }
}

int x64_func::setup_ret_type(c_type ret_type, bool is_signed) {
    filter_type(ret_type, is_signed);
    if(ret_type != C_VOID) {
        flush_register(RAX);
        set_reg_type(RAX, ret_type, is_signed);
        reg_status_list[RAX] = new_id++;
    }
    return reg_status_list[RAX]; 
}

int x64_func::call_function(int exp_id, c_type ret_type, bool is_signed) {
    call_function_begin();
    int reg = fetch_result(exp_id);
    add_inst_to_code(INSTRUCTION("call *%{}", get_register_string(reg_type_list[reg], reg)));
    free_reg(reg);
    call_function_end();

    return setup_ret_type(ret_type, is_signed);
}

int x64_func::call_function(std::string_view name, c_type ret_type, bool is_signed) {
    call_function_begin();
    add_inst_to_code(INSTRUCTION("call {}", name));
    call_function_end();
    
    return setup_ret_type(ret_type, is_signed);
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
