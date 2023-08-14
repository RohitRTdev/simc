#pragma once

#include <vector>
#include <list>
#include <optional>
#include "debug-api.h"
#include "lib/code-gen.h"

#define ALIGN(val, align) (((abs(val)-1) & ~((align)-1))+align)
#define NUM_REGS 6

extern std::vector<std::string> regs_64;
extern std::vector<std::string> regs_32;

enum registers {
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI
};


struct c_expr_x64 {
    bool is_var; //A variable is always loaded into a mem location, but can also be present in a register
    bool is_local; //Tells if it is local or global variable
    bool cached; //Tells if value is loaded into some register or not
    size_t id; //This uniquely identifies this variable or expression
    union {
        int offset; //Gives us info on where var/tmp is loaded in memory(stack)
        size_t address; //Same, but for global memory
    }loc;
    std::optional<c_var> var_info; //This info is only used during debugging
};

class x64_func;

class x64_tu : public Itranslation {
    std::vector<c_var> globals;
    std::vector<std::pair<x64_func*, size_t>>  fn_list;
    size_t global_var_id;
public:
    
    x64_tu();
    
    int declare_global_variable(const std::string& name, c_type type) override;
    int declare_global_variable(const std::string& name, c_type type, std::string_view constant) override;
    Ifunc_translation* add_function(const std::string& name) override;
    void generate_code() override;

    std::string_view fetch_global_variable(int id) const;

    ~x64_tu() override;
};

class x64_func : public Ifunc_translation {

    std::vector<c_expr_x64> id_list;
    size_t reg_status_list[NUM_REGS];
    bool reg_no_clobber_list[NUM_REGS];
    size_t new_id;
    size_t threshold_id;
    int cur_offset;
    x64_tu* parent;
    const std::string fn_name; 
    static int new_label_id;
    int ret_label_id;

    int advance_offset_32() {
        cur_offset = -ALIGN(cur_offset, 4) - 4;
        return cur_offset;
    }

    //This function loads the value in a register and returns the register index
    int fetch_result(int id) {
        sim_log_debug("Fetching result location for id:{}", id);
        
       //Check if result is saved in a register
        for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
            if(reg_status_list[reg_idx] == id) {
                sim_log_debug("Found id saved in register:{}", regs_32[reg_idx]);
                return reg_idx;
            }
        } 
        
        //If not found in register, then find it's location in memory
        for(auto& var : id_list) {
            if(var.id == id) {
                //Check if it is a variable
                if(var.is_var) {
                    int reg = load_var_32(var);
                    return reg;
                }
                //Value of temporary location is cached in memory
                else if(var.cached) {
                    sim_log_debug("Found id in stack");
                    int reg = choose_free_reg(id);
                    add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", var.loc.offset, regs_32[reg]));
                    var.cached = false;

                    return reg;
                }
            }
        }

        CRITICAL_ASSERT_NOW("Non existent expr id was supplied"); 
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

        flush_register(static_cast<registers>(reg));

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


    int load_var_32(const c_expr_x64& var) {
        sim_log_debug("Loading var with var_id:{} -> name:\"{}\"", var.id, var.var_info.value().name);
        int reg = choose_free_reg(var.id);
        int offset = var.loc.offset;
        add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", offset, regs_32[reg]));

        return reg;
    }

    const c_expr_x64& fetch_var(int var_id) const {
        for(const auto& var: id_list) {
            if(var.id == var_id)
                return var;
        }

        CRITICAL_ASSERT_NOW("fetch_var() called with incorrect var_id");
    }

    void flush_register(enum registers reg_idx) {
        
        int offset = 0;
        bool found_existing_cache = false;
        for(auto& loc: id_list) {
            if(loc.is_var && loc.id == reg_status_list[reg_idx]) {
                found_existing_cache = true;
                break;
            }
            else if(!loc.is_var && !loc.cached) {
                sim_log_debug("Found existing free stack location with previous exp_id:{}", loc.id);
                loc.cached = true;
                loc.id = reg_status_list[reg_idx];
                offset = loc.loc.offset;
                found_existing_cache = true;
                break;
            }
        }

        if(!found_existing_cache) {
            c_expr_x64 tmp{};
            tmp.cached = true;
            tmp.id = reg_status_list[reg_idx];
            offset = tmp.loc.offset = advance_offset_32();
            tmp.is_var = false;

            reg_status_list[reg_idx] = 0;
            id_list.push_back(tmp);

        }
        
        sim_log_debug("Freeing {} register to stack at offset:{}", regs_32[reg_idx], offset);

        add_inst_to_code(INSTRUCTION("movl %{}, {}(%rbp)", regs_32[reg_idx], offset));

        free_reg(reg_idx);
    }

    void transfer_to_reg(enum registers reg_idx, int exp_id) {
        
        if(reg_status_list[reg_idx] == exp_id)
            return;
        
        if(reg_status_list[reg_idx]) {
            flush_register(reg_idx);
        }

        for(int i = 0; i < NUM_REGS; i++) {
            if(reg_status_list[i] == exp_id) {
                if(i != reg_idx) {
                    reg_status_list[reg_idx] = exp_id;
                    free_reg(static_cast<registers>(i));
                    add_inst_to_code(INSTRUCTION("movl %{}, %{}", regs_32[i], regs_32[reg_idx]));
                    return;
                }
            }
        }

        for(auto& loc: id_list) {
            if(loc.id == exp_id) {
                if(!loc.is_var) {
                    loc.cached = false;
                }
                reg_status_list[reg_idx] = exp_id;
                add_inst_to_code(INSTRUCTION("movl {}(%rbp), %{}", loc.loc.offset, regs_32[reg_idx]));
                return;
            }
        }

        CRITICAL_ASSERT_NOW("transfer_to_reg() couldn't find exp_id in id_list or reg_status_list");
    }

    void free_reg(enum registers reg_idx) {
        reg_status_list[reg_idx] = 0;
    }

public:

    x64_func(const std::string& name, x64_tu* parent, size_t id_allowed);
//declaration
    int declare_local_variable(const std::string& name, c_type type) override;
    void free_result(int exp_id) override;

//Assign variable
    int assign_var_int(int var_id1, int var_id2) override; 
    int assign_var_int_c(int var_id1, std::string_view constant) override; 
    void assign_to_mem_int(int exp_id, int var_id) override;
    void assign_to_mem_int_c(int exp_id1, std::string_view constant) override;
    int fetch_global_var_int(int id) override;
    int assign_global_var_int(int id, int expr_id) override;
    int assign_global_var_int_c(int id, std::string_view constant) override;

//Addition operation
    int add_int(int id1, int id2) override; 
    int add_int_c(int id, std::string_view constant) override; 

//Branch operation
    int create_label() override;
    void add_label(int label_id) override;
    void branch_return(int exp_id) override;
    void fn_return(int exp_id) override;

    void generate_code() override;

    std::string fetch_fn_name() const {
        return fn_name;
    }

};
