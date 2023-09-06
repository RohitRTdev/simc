#pragma once

#include <algorithm>
#include <array>
#include <tuple>
#include <vector>
#include <list>
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
    c_var var_info; //Gives us crucial information on the properties of the variable
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
            add_inst_to_code(LINE());
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
    std::array<size_t, NUM_REGS> reg_status_list;
    std::array<c_type, NUM_REGS> reg_type_list;
    std::array<bool, NUM_REGS> sign_list;
    std::array<bool, NUM_REGS> reg_no_clobber_list;
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
        
                
        auto pos = std::find(reg_status_list.begin(), reg_status_list.end(), id);
        if(pos != reg_status_list.end()) {
            size_t reg_idx = pos - reg_status_list.begin();
            sim_log_debug("Found id saved in register:{}", regs[0][reg_idx]);
            return reg_idx;
        }
        
        //If not found in register, then find it's location in memory
        for(auto& var : id_list) {
            if(var.id == id) {
                //Check if it is a variable
                if(var.is_var) {
                    int reg = 0;
                    if(var.var_info.mem_var_size.has_value()) {
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
                    auto type = var.var_info.type;
                    int reg = choose_free_reg(type, var.var_info.is_signed, var.id);
                    insert_code("mov{} {}(%rbp), %{}", type, std::to_string(var.offset), reg);
                    var.cached = false;

                    return reg;
                }
            }
        }

        CRITICAL_ASSERT_NOW("Non existent expr id was supplied"); 
        return 0;
    }

    void allocate_exp_id(int exp_id, c_type base_type, bool is_signed, int reg) {
        if(!exp_id) {
            reg_status_list[reg] = new_id++;
            set_reg_type(reg, base_type, is_signed);
            sim_log_debug("Allocating new exp_id:{} for register:{}", reg_status_list[reg], regs[0][reg]);
        } else {
            sim_log_debug("Using exp_id:{} for register:{}", exp_id, regs[0][reg]);
            reg_status_list[reg] = exp_id;
            set_reg_type(reg, base_type, is_signed);
        }
    }

    int save_and_free_reg(c_type base_type, bool is_signed, int exp_id = 0) {
        
        //First select a register not present in no_clobber_list
        int reg = 0;
        auto pos = std::find(reg_no_clobber_list.begin(), reg_no_clobber_list.end(), false);
        if(pos != reg_no_clobber_list.end()) {
            reg = pos - reg_no_clobber_list.begin();
        }
        else {
            CRITICAL_ASSERT_NOW("reg allocation failed as save_and_free_reg() could not find/flush any register");
        }

        flush_register(reg);
        allocate_exp_id(exp_id, base_type, is_signed, reg);
        return reg;   
    }

    void set_reg_type(int reg_idx, c_type base_type, bool is_signed) {
        reg_type_list[reg_idx] = base_type;
        sign_list[reg_idx] = is_signed;
    }

    int choose_free_reg(c_type base_type, bool is_signed, int exp_id = 0) {
        
        auto pos = std::find(reg_status_list.begin(), reg_status_list.end(), 0);
        if(pos != reg_status_list.end()) {
            int reg_index = pos - reg_status_list.begin();
            sim_log_debug("Found new free register:{}", regs[0][reg_index]);
            allocate_exp_id(exp_id, base_type, is_signed, reg_index);
            return reg_index;
        }

        sim_log_debug("No free register found");
        return save_and_free_reg(base_type, is_signed, exp_id);
    }

    int load_mem_var(const c_expr_x64& var) {
        sim_log_debug("Loading memory var with var_id:{} -> name:{} and size:{}", var.id, var.var_info.name, var.var_info.mem_var_size.value());
        int reg = choose_free_reg(C_LONG, false, var.id);
        int offset = var.offset;

        insert_code("lea{} {}(%rbp), %{}", C_LONG, std::to_string(offset), reg);
        return reg;
    }

    int load_var(const c_expr_x64& var) {
        sim_log_debug("Loading var with var_id:{} -> name:\"{}\"", var.id, var.var_info.name);
        int reg = choose_free_reg(var.var_info.type, var.var_info.is_signed, var.id);
        int offset = var.offset;
        auto type = var.var_info.type;
        insert_code("mov{} {}(%rbp), %{}", type, std::to_string(offset), reg);

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

    void flush_register(int reg_idx) {
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
                loc.var_info.type = reg_type_list[reg_idx];
                loc.var_info.is_signed = sign_list[reg_idx];
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
            tmp.var_info.is_signed = sign_list[reg_idx];
            tmp.var_info.type = reg_type_list[reg_idx];
            reg_status_list[reg_idx] = 0;
        
            id_list.push_back(tmp);
        }
        
        sim_log_debug("Freeing {} register to stack at offset:{}", regs[0][reg_idx], offset);

        auto type = reg_type_list[reg_idx];
        insert_code("mov{} %{}, {}(%rbp)", type, reg_idx, std::to_string(offset));

        free_reg(reg_idx);
    }

    void transfer_to_reg(enum registers reg_idx, int exp_id) {
        
        if(reg_status_list[reg_idx] == exp_id)
            return;

        CRITICAL_ASSERT(!reg_no_clobber_list[reg_idx], "tranfer_to_reg() failed as reg_idx:{} is in no_clobber_list", reg_idx);
        
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

                    insert_code("mov{} %{}, %{}", type, i, reg_idx);    
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
                set_reg_type(reg_idx, loc.var_info.type, loc.var_info.is_signed);
                
                auto type = reg_type_list[reg_idx];
                insert_code("mov{} {}(%rbp), %{}", type, std::to_string(loc.offset), reg_idx);
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
        reg_no_clobber_list[reg_idx] = false;
        reg_status_list[reg_idx] = 0;
    }

    std::tuple<c_type, bool> fetch_result_type(int exp_id) {
        for(int i = 0; i < NUM_REGS; i++) {
            if(reg_status_list[i] == exp_id) {
                return std::make_tuple(reg_type_list[i], sign_list[i]);
            }            
        }

        for(const auto& var: id_list) {
            if(var.is_var || var.cached) {
                if(var.id == exp_id) {
                    return std::make_tuple(var.var_info.type, var.var_info.is_signed);
                }
            }
        }

        CRITICAL_ASSERT_NOW("fetch_result_type() called with invalid exp_id:{}", exp_id);
    }

    void free_preferred_register(int reg_idx, c_type type, bool is_signed) {
        if(reg_status_list[reg_idx]) {
            int i;
            for (i = 0; i < NUM_REGS; i++) {
                if (i != reg_idx && !reg_status_list[i]) {
                    //We found a free register
                    insert_code("mov{} %{}, %{}", type, reg_idx, i);
                    set_reg_type(i, reg_type_list[reg_idx], sign_list[reg_idx]);
                    reg_status_list[i] = reg_status_list[reg_idx];
                    break;
                }
            }
            if(i == NUM_REGS) {
                //We didn't find a free register
                flush_register(static_cast<registers>(reg_idx));
            }
        }   

        reg_status_list[reg_idx] = new_id++;
        set_reg_type(reg_idx, type, is_signed);
    }

    std::string_view make_format_args(c_type type, std::string_view constant) {
        return constant;
    }
    
    std::string_view make_format_args(c_type type, int reg) {
        return get_register_string(type, reg);
    }

    template<typename... Args>
    void insert_code(std::string_view msg, c_type type, Args&&... args) {
        std::string _msg = "\t" + std::string(msg) + "\n";
        add_inst_to_code(fmt::format(_msg, inst_suffix[type], make_format_args(type, args)...));
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
    int mul(int id1, int id2) override;
    int mul(int id1, std::string_view constant) override;
    
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
