#pragma once

#include <tuple>
#include <vector>
#include <list>
#include <optional>
#include "debug-api.h"
#include "lib/code-gen.h"

#define ALIGN(val, align) (((abs(val)-1) & ~((align)-1))+align)
#define NUM_REGS 6

extern const std::vector<std::array<std::string, NUM_REGS>> regs;
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
    int offset; //Gives us info on where var/tmp is loaded in memory(stack)
    std::optional<c_var> var_info; //This info is only used during debugging
};

class x64_func;


class x64_tu : public Itranslation {
    std::vector<c_var> globals;
    std::vector<std::pair<x64_func*, size_t>> fn_list;
    size_t global_var_id;


    enum Segment {
        DATA,
        BSS
    };

    template<Segment segment>
    void write_segment(std::string& data) {
        if(data.size()) {
            const char* seg_name = nullptr;
            if constexpr (segment == DATA) {
                seg_name = LINE(".section .data");
            }
            else if constexpr (segment == BSS) {
                seg_name = LINE(".section .bss");
            }
            data = seg_name + data;
            add_inst_to_code(data);
        }
    }

public:
    
    x64_tu();
    
    int declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static) override;
    int declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static, std::string_view constant) override;
    int declare_global_mem_variable(std::string_view name, bool is_static, size_t mem_var_size) override; 
    Ifunc_translation* add_function(std::string_view name, c_type ret_type, bool is_signed, bool is_static) override; 
    void generate_code() override;

    const c_var& fetch_global_variable(int id) const;

    ~x64_tu() override;
};

class x64_func : public Ifunc_translation {

    std::vector<c_expr_x64> id_list;
    size_t reg_status_list[NUM_REGS];
    c_type reg_type_list[NUM_REGS];
    bool sign_list[NUM_REGS];
    bool reg_no_clobber_list[NUM_REGS];
    size_t new_id;
    int cur_offset;
    x64_tu* parent;
    std::string_view fn_name; 
    static int new_label_id;
    int ret_label_id;

    constexpr static const std::array<char, NUM_TYPES> inst_suffix = {'l', 'b', 'q', 'w', 'q'};


    using op_type = std::tuple<int, int, int, c_type>; 

    int advance_offset(c_type type) {
        int alignment = base_type_size(type);
        cur_offset = -ALIGN(cur_offset, alignment) - alignment;
        return cur_offset;
    }

    int advance_offset(size_t mem_var_size) {
        cur_offset -= mem_var_size;
        return cur_offset;
    }

    //This function loads the value in a register and returns the register index
    int fetch_result(int id) {
        sim_log_debug("Fetching result location for id:{}", id);
        
       //Check if result is saved in a register
        for(size_t reg_idx = 0; reg_idx < NUM_REGS; reg_idx++) {
            if(reg_status_list[reg_idx] == id) {
                sim_log_debug("Found id saved in register:{}", regs[0][reg_idx]);
                return reg_idx;
            }
        } 
        
        //If not found in register, then find it's location in memory
        for(auto& var : id_list) {
            if(var.id == id) {
                //Check if it is a variable
                if(var.is_var) {
                    int reg = 0;
                    if(var.var_info.value().mem_var_size.has_value()) {
                        reg = load_mem_var(var);
                    }
                    else {
                        reg = load_var(var);
                    }
                    return reg;
                }
                //Value of temporary location is cached in memory
                else if(var.cached) {
                    sim_log_debug("Found id in stack");
                    auto type = var.var_info.value().type;
                    int reg = choose_free_reg(type, var.var_info.value().is_signed, var.id);
                    add_inst_to_code(INSTRUCTION("mov{} {}(%rbp), %{}", inst_suffix[type], var.offset, get_register_string(type, reg)));
                    var.cached = false;

                    return reg;
                }
            }
        }

        CRITICAL_ASSERT_NOW("Non existent expr id was supplied"); 
        return 0;
    }

    int save_and_free_reg(c_type base_type, bool is_signed, int exp_id = 0) {
        
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
            set_reg_type(reg, base_type, is_signed);
            sim_log_debug("Allocating new exp_id:{} for register:{}", reg_status_list[reg], regs[0][reg]);
        } else {
            sim_log_debug("Using exp_id:{} for register:{}", exp_id, regs[0][reg]);
            reg_status_list[reg] = exp_id;
            set_reg_type(reg, base_type, is_signed);
        }

        return reg;   
    }

    void set_reg_type(int reg_idx, c_type base_type, bool is_signed) {
        reg_type_list[reg_idx] = base_type;
        sign_list[reg_idx] = is_signed;
    }

    int choose_free_reg(c_type base_type, bool is_signed, int exp_id = 0) {
        int reg_index = 0;
        for(auto& reg: reg_status_list) {
            if(!reg) {
                sim_log_debug("Found new free register:{}", regs[0][reg_index]);
                if(!exp_id) {
                    reg = new_id++;
                    set_reg_type(reg_index, base_type, is_signed);
                    sim_log_debug("Allocating new exp_id:{}", reg);
                } else {
                    reg = exp_id;   
                    set_reg_type(reg_index, base_type, is_signed);
                    sim_log_debug("Using exp_id:{}", exp_id);
                }
                return reg_index;
            }
            reg_index++;
        }

        sim_log_debug("No free register found");
        return save_and_free_reg(base_type, is_signed, exp_id);
    }

    int load_mem_var(const c_expr_x64& var) {
        sim_log_debug("Loading memory var with var_id:{} -> name:{} and size:{}", var.id, var.var_info.value().name, var.var_info.value().mem_var_size.value());
        int reg = choose_free_reg(C_LONG, false, var.id);
        int offset = var.offset;
        add_inst_to_code(INSTRUCTION("lea{} {}(%rbp), %{}", inst_suffix[C_LONG], offset, get_register_string(C_LONG, reg)));

        return reg;
    }

    int load_var(const c_expr_x64& var) {
        sim_log_debug("Loading var with var_id:{} -> name:\"{}\"", var.id, var.var_info.value().name);
        int reg = choose_free_reg(var.var_info.value().type, var.var_info.value().is_signed, var.id);
        int offset = var.offset;
        auto type = var.var_info.value().type;
        add_inst_to_code(INSTRUCTION("mov{} {}(%rbp), %{}", inst_suffix[type], offset, get_register_string(type, reg)));

        return reg;
    }

    const c_expr_x64& fetch_var(int var_id) const {
        for(const auto& var: id_list) {
            if(var.id == var_id)
                return var;
        }

        CRITICAL_ASSERT_NOW("fetch_var() called with incorrect var_id");
    }

    std::string_view get_register_string(c_type base_type, int reg_idx) {
        switch(base_type) {
            case C_INT: return regs[1][reg_idx]; break;
            case C_CHAR: return regs[3][reg_idx]; break;
            case C_SHORT: return regs[2][reg_idx]; break;
            case C_LONG:
            case C_LONGLONG: return regs[0][reg_idx]; break;  
            default: CRITICAL_ASSERT_NOW("Invalid type specified in get_register_string()");
        }
    }

    void type_mismatch_check(int reg1, int reg2) {
            CRITICAL_ASSERT((reg_type_list[reg1] == reg_type_list[reg2])
            && (sign_list[reg1] == sign_list[reg2]), 
            "type mismatch occured between exp_id:{} and exp_id:{} of type:{} and type:{}", reg_status_list[reg1], reg_status_list[reg2], reg_type_list[reg1], reg_type_list[reg2]);
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
                offset = loc.offset;
                loc.var_info.value().type = reg_type_list[reg_idx];
                loc.var_info.value().is_signed = sign_list[reg_idx];
                found_existing_cache = true;
                break;
            }
        }

        if(!found_existing_cache) {
            c_expr_x64 tmp{};
            tmp.cached = true;
            tmp.id = reg_status_list[reg_idx];
            offset = tmp.offset = advance_offset(reg_type_list[reg_idx]);
            tmp.is_var = false;
            tmp.var_info->is_signed = sign_list[reg_idx];
            tmp.var_info->type = reg_type_list[reg_idx];
            reg_status_list[reg_idx] = 0;
        
            id_list.push_back(tmp);
        }
        
        sim_log_debug("Freeing {} register to stack at offset:{}", regs[0][reg_idx], offset);

        auto type = reg_type_list[reg_idx];
        add_inst_to_code(INSTRUCTION("mov{} %{}, {}(%rbp)", inst_suffix[type], get_register_string(type, reg_idx), offset));

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
                    set_reg_type(reg_idx, reg_type_list[i], sign_list[i]);
                    free_reg(i);
                    
                    auto type = reg_type_list[reg_idx];
                    auto reg1_str = get_register_string(type, i);
                    auto reg2_str = get_register_string(type, reg_idx); 
                    
                    add_inst_to_code(INSTRUCTION("mov{} %{}, %{}", inst_suffix[type], reg1_str, reg2_str));
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
                set_reg_type(reg_idx, loc.var_info.value().type, loc.var_info.value().is_signed);
                
                auto type = reg_type_list[reg_idx];
                add_inst_to_code(INSTRUCTION("mov{} {}(%rbp), %{}", inst_suffix[type], loc.offset, get_register_string(type, reg_idx)));
                return;
            }
        }

        CRITICAL_ASSERT_NOW("transfer_to_reg() couldn't find exp_id:{} in id_list or reg_status_list", exp_id);
    }

    op_type unary_op_fetch(int id) {
        int reg1 = fetch_result(id);
        reg_no_clobber_list[reg1] = true;

        auto type = reg_type_list[reg1];
        int reg = choose_free_reg(type, sign_list[reg1]);

        return std::make_tuple(reg1, 0, reg, type);
    }

    op_type binary_op_fetch(int id1, int id2) {

        int reg1 = fetch_result(id1);
        reg_no_clobber_list[reg1] = true;
        int reg2 = fetch_result(id2);
        reg_no_clobber_list[reg2] = true;
      
        type_mismatch_check(reg1, reg2);

        auto type = reg_type_list[reg1];
        int reg = choose_free_reg(type, sign_list[reg1]);

        return std::make_tuple(reg1, reg2, reg, type);
    }

    void free_reg(int reg_idx) {
        reg_status_list[reg_idx] = 0;
    }

public:

    x64_func(std::string_view name, x64_tu* parent);
//declaration
    int declare_local_variable(std::string_view name, c_type type, bool is_signed) override;
    int declare_local_mem_variable(std::string_view name, size_t mem_var_size) override; 
    void free_result(int exp_id) override;

//Assign variable
    int assign_var(int var_id1, int var_id2) override; 
    int assign_var(int var_id1, std::string_view constant) override; 
    int assign_to_mem(int exp_id, int var_id) override;
    int assign_to_mem(int exp_id1, std::string_view constant, c_type type) override;
    int fetch_global_var(int id) override;
    int assign_global_var(int id, int expr_id) override;
    int assign_global_var(int id, std::string_view constant) override;

//Arithmetic operations
    int add(int id1, int id2) override; 
    int add(int id, std::string_view constant) override; 
    int sub(int id1, int id2) override;
    int sub(int id, std::string_view constant) override;
    int sub(std::string_view constant, int id) override;
    
//Branch operation
    int create_label() override;
    void add_label(int label_id) override;
    void branch_return(int exp_id) override;
    void fn_return(int exp_id) override;

    void generate_code() override;

    std::string_view fetch_fn_name() const {
        return fn_name;
    }

};
