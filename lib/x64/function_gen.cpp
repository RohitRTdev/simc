#include <list>
#include <spdlog/fmt/fmt.h>
#include "debug-api.h"
#include "compiler/code-gen.h"

#define ALIGN(val, align) ((((val)-1) & ~((align)-1))+align)

static const char* regs_64[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi"};
static const char* regs_32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi"};


#define NUM_REGS (sizeof(regs_64) / sizeof(regs_64[0]))

// enum reg_id {
//     rax,
//     rbx,
//     rcx,
//     rdx,
//     rbp,
//     rsp,
//     rsi,
//     rdi
// };


struct c_var_x64 {
    c_var var_info;
    bool is_local;

    union {
        int offset;
        size_t address;    
    }loc;

};

struct c_var_x64_tmp {
    size_t id;
    int offset;    
};

class x64_func : public Ifunc_translation {

    std::vector<c_var_x64> vars;
    std::list<c_var_x64_tmp> tmp;
    size_t reg_status_list[NUM_REGS];
    size_t new_id, var_id;
    int cur_offset;

    int advance_offset_32() {
        cur_offset = ALIGN(cur_offset, 4) - 4;
        return cur_offset;
    }

    int fetch_result(int id) {
        sim_log_debug("Fetching result location for exp_id:{}", id);
        for(auto var = tmp.begin(); var != tmp.end(); var++) {
            if(var->id == id) {
                sim_log_debug("Found id in stack");
                int reg = choose_free_reg(id);
                add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", var->offset, regs_32[reg]));
                tmp.erase(var);

                return reg;
            }
        }


        //This means the result is saved in a register
        for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
            if(reg_status_list[reg_idx] == id) {
                sim_log_debug("Found id saved in register:{}", regs_32[reg_idx]);
                return reg_idx;
            }
        }

        //Control should never reach here
        return 0;

    }

    int save_and_free_reg(int exp_id = 0) {
        int offset = advance_offset_32();
        tmp.push_back({reg_status_list[0], offset});
        sim_log_debug("Freeing {} register to stack at offset:{}", regs_32[0], offset);

        add_inst_to_code(INSTRUCTION("movl %{}, {}(%rbp)", regs_32[0], offset));

        if(!exp_id) {
            reg_status_list[0] = new_id++;
            sim_log_debug("Allocating new exp_id:{} for register:{}", reg_status_list[0], regs_32[0]);
        } else {
            sim_log_debug("Using exp_id:{} for register:{}", exp_id, regs_32[0]);
            reg_status_list[0] = exp_id;
        }

        return 0;   
    }

    int choose_free_reg(int exp_id = 0) {
        int reg_index = 0;
        for(auto& reg: reg_status_list) {
            if(!reg) {
                sim_log_debug("Found new free register:{}", regs_32[reg_index]);
                if(!exp_id) {
                    reg = new_id++;
                    sim_log_debug("Allocating new exp_id:{}", reg);
                } else {
                    reg = exp_id;   
                    sim_log_debug("Using exp_id:{}", exp_id);
                }
                return reg_index;
            }
            reg_index++;
        }

        sim_log_debug("No free register found");
        return save_and_free_reg(exp_id);
    }

    void free_reg(int reg) {
        reg_status_list[reg] = 0;
    }


    int load_var_32(int var_id) {
        sim_log_debug("Loading var with var_id:{}", var_id);
        int reg = choose_free_reg();
        int offset = vars[var_id].loc.offset;
        add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", offset, regs_32[reg]));

        return reg;
    }

public:

    x64_func();
//declaration
    int declare_local_variable(const std::string& name, c_type type);

//Assign variable
    void assign_ii(int var_id1, int var_id2) override; 
    void assign_ic(int var_id1, int constant) override; 
    void assign_ir(int var_id, int exp_id) override; 

//Addition operation
    int add_ii(int var_id1, int var_id2) override; 
    int add_ic(int var_id, int constant) override; 
    int add_ri(int exp_id, int var_id) override; 
    int add_rc(int exp_id, int constant) override; 
    
    void generate_code() override;

};

x64_func::x64_func(): new_id(1), cur_offset(0), var_id(0) {
    //0 indicates reg is free
    for(auto& reg: reg_status_list) 
        reg = 0;
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

void x64_func::generate_code() {
    std::string frame1 = INSTRUCTION("pushq %{}", "rbp"); 
    std::string frame2 = INSTRUCTION("movq %{}, %{}", "rsp", "rbp");

    cur_offset = ALIGN(cur_offset, 16);
    std::string frame3;
    if(cur_offset)
        frame3 = INSTRUCTION("subq ${}, %{}", -cur_offset, "rsp"); 

    std::string endframe = "\tleave\n\tret\n";

    code = frame1 + frame2 + frame3 + code + endframe; 
}


Ifunc_translation* create_function_translation_unit() {
    auto* unit = new x64_func();

    return unit;
}