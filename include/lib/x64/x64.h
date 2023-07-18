#pragma once

#include <vector>
#include <list>
#include "debug-api.h"
#include "lib/code-gen.h"

#define ALIGN(val, align) (((abs(val)-1) & ~((align)-1))+align)
#define NUM_REGS 6

extern const char* regs_64[];
extern const char* regs_32[];

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
    bool cached;
    int offset;    
};

class x64_func : public Ifunc_translation {

    std::vector<c_var_x64> vars;
    std::list<c_var_x64_tmp> tmp;
    size_t reg_status_list[NUM_REGS];
    bool reg_no_clobber_list[NUM_REGS];
    size_t new_id, var_id;
    int cur_offset;

    int advance_offset_32() {
        cur_offset = -ALIGN(cur_offset, 4) - 4;
        return cur_offset;
    }

    int fetch_result(int id) {
        sim_log_debug("Fetching result location for exp_id:{}", id);
        for(auto var = tmp.begin(); var != tmp.end(); var++) {
            if(var->id == id && var->cached) {
                sim_log_debug("Found id in stack");
                int reg = choose_free_reg(id);
                add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", var->offset, regs_32[reg]));
                var->cached = false;

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
        
        //First select a register not present in no_clobber_list
        int reg = 0;
        for(auto _reg: reg_no_clobber_list) {
            if(!_reg) {
                break;
            }
            reg++;
        }

        int offset = 0;
        bool found_existing_cache = false;
        for(auto& loc: tmp) {
            if(!loc.cached) {
                sim_log_debug("Found existing free stack location with previous exp_id:{}", loc.id);
                loc.cached = true;
                loc.id = reg_status_list[reg];
                offset = loc.offset;

                found_existing_cache = true;
                break;
            }
        }
        if(!found_existing_cache) {
            offset = advance_offset_32();
            tmp.push_back({reg_status_list[reg], true, offset});

        }
        
        sim_log_debug("Freeing {} register to stack at offset:{}", regs_32[reg], offset);

        add_inst_to_code(INSTRUCTION("movl %{}, {}(%rbp)", regs_32[reg], offset));

        if(!exp_id) {
            reg_status_list[reg] = new_id++;
            sim_log_debug("Allocating new exp_id:{} for register:{}", reg_status_list[reg], regs_32[reg]);
        } else {
            sim_log_debug("Using exp_id:{} for register:{}", exp_id, regs_32[reg]);
            reg_status_list[reg] = exp_id;
        }

        return reg;   
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


    int load_var_32(int var_id) {
        sim_log_debug("Loading var with var_id:{} -> name:\"{}\"", var_id, vars[var_id].var_info.name);
        int reg = choose_free_reg();
        int offset = vars[var_id].loc.offset;
        add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", offset, regs_32[reg]));

        return reg;
    }

public:

    x64_func();
//declaration
    int declare_local_variable(const std::string& name, c_type type) override;

//Assign variable
    void assign_ii(int var_id1, int var_id2) override; 
    void assign_ic(int var_id1, int constant) override; 
    void assign_ir(int var_id, int exp_id) override; 
    void assign_to_mem_i(int exp_id, int var_id) override;
    void assign_to_mem_r(int exp_id1, int exp_id2) override;
    void assign_to_mem_c(int exp_id, int constant) override;

//Addition operation
    int add_ii(int var_id1, int var_id2) override; 
    int add_ic(int var_id, int constant) override; 
    int add_ri(int exp_id, int var_id) override; 
    int add_rc(int exp_id, int constant) override; 
    int add_rr(int exp_id1, int exp_id2) override;
    
    void generate_code() override;
};
