#pragma once

#include <algorithm>
#include <stack>
#include <functional>
#include <array>
#include <tuple>
#include <vector>
#include <list>
#include "debug-api.h"
#include "lib/code-gen.h"

#define ALIGN(val, align) (((abs(val)-1) & ~((align)-1))+align)
#define NUM_REGS 8
#define NUM_CALL_REGS 6


extern const std::vector<std::array<std::string, NUM_REGS>> regs;
enum registers {
    RAX,
    RBX,
    RCX,
    RDX,
    RSI,
    RDI,
    R8,
    R9
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

void filter_type(c_type& type, bool& is_signed);
void filter_type(c_type& type);

class x64_tu : public Itranslation {
    std::vector<c_var> globals;
    std::vector<std::pair<x64_func*, size_t>> fn_list;
    size_t global_var_id;


    enum Segment {
        DATA,
        BSS,
        RODATA
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
            else if constexpr (segment == RODATA) {
                seg_name = LINE(".section .rodata");
            }
            data = seg_name + data;
            add_inst_to_code(data);
            add_inst_to_code(LINE());
        }
    }

public:
    
    x64_tu();
    
    int declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static) override;
    void init_variable(int var_id, std::string_view constant) override;
    int declare_global_mem_variable(std::string_view name, bool is_static, size_t mem_var_size) override; 
    int declare_string_constant(std::string_view name, std::string_view value) override;
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
    int ret_label_id;
    c_type m_ret_type;
    bool m_is_signed;
    size_t num_call_args;
    size_t call_index;
    size_t bytes_moved;
    size_t param_offset;
    constexpr static const std::array<registers, NUM_CALL_REGS> reg_call_list = {RDI, RSI, RDX, RCX, R8, R9};
    std::array<int, NUM_REGS - NUM_CALL_REGS> reg_non_call_list;
    constexpr static const std::array<char, NUM_TYPES> inst_suffix = {'b', 'w', 'l', 'q', 'q'};

    using param_stack_type = std::variant<int, std::tuple<std::string_view, c_type, bool>>; 
    std::stack<param_stack_type> param_stack;

    using op_type = std::tuple<int, int, c_type>; 

    int advance_param_offset() {
        const int param_alignment = 8;
        param_offset += param_alignment;
        return param_offset;
    }

    int advance_offset(c_type type) {
        int alignment = base_type_size(type);
        cur_offset = -ALIGN(cur_offset, alignment) - alignment;
        return cur_offset;
    }

    int advance_offset(size_t mem_var_size) {
        cur_offset -= mem_var_size;
        return cur_offset;
    }

    bool is_type_lower(c_type type1, c_type type2) {
        if(type1 == C_LONGLONG)
            type1 = C_LONG;
        if(type2 == C_LONGLONG)
            type2 = C_LONG;


        return type1 <= type2;
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
        int reg = choose_free_reg(C_LONG, false);
        int offset = var.offset;

        insert_code("lea{} {}(%rbp), %{}", C_LONG, std::to_string(offset), reg);
        return reg;
    }

    int load_var(const c_expr_x64& var) {
        sim_log_debug("Loading var with var_id:{} -> name:\"{}\"", var.id, var.var_info.name);
        int reg = choose_free_reg(var.var_info.type, var.var_info.is_signed);
        int offset = var.offset;
        auto type = var.var_info.type;
        insert_code("mov{} {}(%rbp), %{}", type, std::to_string(offset), reg);

        return reg;
    }

    const std::optional<c_expr_x64> fetch_var(int var_id, bool avoid_mem_var = false) const {
        auto pos = std::find_if(id_list.begin(), id_list.end(), [&] (const auto& var){
            return var_id == var.id && var.is_var && ((avoid_mem_var && !var.var_info.mem_var_size) || (!avoid_mem_var));
        });

        if(pos != id_list.end()) {
            return *pos;
        }

        sim_log_debug("fetch_var() couldn't find var with var_id:{}", var_id);
        return std::optional<c_expr_x64>();
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
        if(!reg_status_list[reg_idx])
            return;

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
        
        if(reg_status_list[reg_idx]) {
            flush_register(reg_idx);
        }

        for(int i = 0; i < reg_status_list.size(); i++) {
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

                if(loc.is_var) {
                    if(loc.var_info.mem_var_size)
                        insert_code("lea{} {}(%rbp), %{}", C_LONG, std::to_string(loc.offset), reg_idx);
                    else
                        insert_code("mov{} {}(%rbp), %{}", loc.var_info.type, std::to_string(loc.offset), reg_idx);
                    reg_status_list[reg_idx] = new_id++;
                }
                else {
                    insert_code("mov{} {}(%rbp), %{}", loc.var_info.type, std::to_string(loc.offset), reg_idx);
                    reg_status_list[reg_idx] = exp_id;
                }
                set_reg_type(reg_idx, loc.var_info.type, loc.var_info.is_signed);
                return;
            }
        }

        CRITICAL_ASSERT_NOW("transfer_to_reg() couldn't find exp_id:{} in id_list or reg_status_list", exp_id);
    }

    void push_to_stack(param_stack_type stack_object) {
        auto update_offset = [&] () {
            cur_offset -= 8;
            bytes_moved += 8;
        };
        
        if(std::holds_alternative<int>(stack_object)) {
            auto [reg, _, type] = unary_op_fetch(std::get<int>(stack_object));
            update_offset();
            insert_code("push{} %{}", C_LONG, reg);
            free_reg(reg);        
        }
        else {
            auto [constant, type, is_signed] = std::get<1>(stack_object);
            update_offset();
            insert_code("push{} ${}", C_LONG, constant);
        }
    }


    op_type unary_op_fetch(int id) {
        int reg1 = fetch_result(id);

        auto type = reg_type_list[reg1];

        return std::make_tuple(reg1, 0, type);
    }

    op_type binary_op_fetch(int id1, int id2) {

        int reg1 = fetch_result(id1);
        reg_no_clobber_list[reg1] = true;
        int reg2 = fetch_result(id2);
      
        type_mismatch_check(reg1, reg2);

        auto type = reg_type_list[reg1];
        reg_no_clobber_list[reg1] = false;
        return std::make_tuple(reg1, reg2, type);
    }

    void free_reg(int reg_idx) {
        reg_no_clobber_list[reg_idx] = false;
        reg_status_list[reg_idx] = 0;
    }

    std::tuple<c_type, bool> fetch_result_type(int exp_id) {
        for(int i = 0; i < reg_status_list.size(); i++) {
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

    int load_constant(std::string_view constant, c_type type, bool is_signed, int pref_reg = 0) {
        int reg = pref_reg;
        if(!reg)
            reg = choose_free_reg(type, is_signed);
        insert_code("mov{} ${}, %{}", type, constant, reg);

        return reg;
    }

    std::pair<int, int> arg_fetch(int id1, c_type type, bool is_signed, std::variant<int, std::string_view> object, bool in_order, 
    std::function<void (int)>&& lambda = [] (int reg) {}) {
        int reg1 = 0;
        if(!in_order) {
            reg1 = load_constant(std::get<1>(object), type, is_signed);
        }
        else {
            reg1 = std::get<0>(unary_op_fetch(id1));
        }

        lambda(reg1);

        int reg2 = 0;
        if(in_order) {
            if(std::holds_alternative<int>(object)) {
                reg2 = fetch_result(std::get<0>(object));
            }
            else {
                reg2 = load_constant(std::get<1>(object), type, is_signed);
            }
        }
        else {
            reg2 = fetch_result(id1);
        }

        return std::make_pair(reg1, reg2);
    }

    void insert_comment(std::string_view comment) {
    #ifdef SIMDEBUG
        add_inst_to_code(fmt::format(LINE("/*{}*/"), comment));
    #endif
    }

    std::string_view make_format_args(c_type type, std::string_view constant) {
        return constant;
    }
    
    std::string_view make_format_args(c_type type, int reg) {
        return get_register_string(type, reg);
    }

    template<typename... Args>
    void insert_code(std::string_view msg, c_type type, Args&&... args) {
        add_inst_to_code(fmt::format(fmt::runtime("\t"+std::string(msg)+"\n"), inst_suffix[type], make_format_args(type, args)...));
    }

    std::string generate_static_name(const std::string& name) {
        return "." + name + std::to_string(static_id++);
    }

    enum compare_op {
        GT,
        LT,
        EQ,
        NEQ
    };

    int inc_common(int id, c_type type, bool is_signed, size_t inc_count, bool is_pre, bool inc, bool is_mem, bool is_global);
    void call_function_begin();
    void call_function_end();
    int setup_ret_type(c_type ret_type, bool is_signed);
    int if_common(int id1, std::variant<int, std::string_view> object, compare_op op);
    int div_common(int id1, std::variant<int, std::string_view> object, bool is_div, bool in_order = true);
    int and_or_xor_common(int id1, std::variant<int, std::string_view> object, bool is_and, bool is_or);
    int shift_common(int id1, std::variant<int, std::string_view> object, bool is_left, bool in_order = true);

public:
    static int new_label_id;
    static int static_id;
    x64_func(std::string_view name, x64_tu* parent, c_type ret_type, bool is_signed);
//declaration
    int declare_local_variable(std::string_view name, c_type type, bool is_signed, bool is_static) override;
    int declare_local_mem_variable(std::string_view name, bool is_static, size_t mem_var_size) override; 
    int declare_string_constant(std::string_view name, std::string_view value) override;
    void init_variable(int var_id, std::string_view constant) override;
    void free_result(int exp_id) override;
    int create_temporary_value(c_type type, bool is_signed) override;
    int set_value(int expr_id, std::string_view constant) override;

//function
    int save_param(std::string_view name, c_type type, bool is_signed) override;
    void begin_call_frame(size_t num_args);
    void load_param(int exp_id) override;
    void load_param(std::string_view constant, c_type type, bool is_signed) override;
    int call_function(int exp_id, c_type ret_type, bool is_signed) override;
    int call_function(std::string_view name, c_type ret_type, bool is_signed) override;


//Assign variable
    int assign_var(int var_id1, int var_id2) override; 
    int assign_var(int var_id1, std::string_view constant) override; 
    int assign_to_mem(int exp_id, int var_id) override;
    int assign_to_mem(int exp_id1, std::string_view constant, c_type type) override;
    int fetch_from_mem(int id, c_type type, bool is_signed) override;
    int fetch_from_mem(std::string_view constant, c_type type, bool is_signed) override;
    int fetch_global_var(int id) override;
    int assign_global_var(int id, int expr_id) override;
    int assign_global_var(int id, std::string_view constant) override;
    int get_address_of(std::string_view constant) override;
    int get_address_of(int id, bool is_mem, bool is_global) override;

//Arithmetic operations
    int add(int id1, int id2) override; 
    int add(int id, std::string_view constant) override; 
    int sub(int id1, int id2) override;
    int sub(int id, std::string_view constant) override;
    int sub(std::string_view constant, int id) override;
    int mul(int id1, int id2) override;
    int mul(int id1, std::string_view constant) override;
    int div(int id1, int id2) override;
    int div(int id1, std::string_view constant) override;
    int div(std::string_view constant, int id) override;
    int modulo(int id1, int id2) override;
    int modulo(int id1, std::string_view constant) override;
    int modulo(std::string_view constant, int id) override;
    int bit_and(int id1, int id2) override;
    int bit_and(int id1, std::string_view constant) override;
    int bit_or(int id1, int id2) override;
    int bit_or(int id1, std::string_view constant) override;
    int bit_xor(int id1, int id2) override;
    int bit_xor(int id1, std::string_view constant) override;
    int bit_not(int id) override;
    int sl(int id1, int id2) override;
    int sl(int id1, std::string_view constant) override;
    int sl(std::string_view constant, int id) override;
    int sr(int id1, int id2) override;
    int sr(int id1, std::string_view constant) override;
    int sr(std::string_view constant, int id) override;
    int pre_inc(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) override;
    int pre_dec(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) override;
    int post_inc(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) override;
    int post_dec(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) override;
    int negate(int id) override;
//Type conversion
    int type_cast(int exp_id, c_type cast_type, bool cast_sign) override;

//Conditional operation
    int if_gt(int id1, int id2) override;
    int if_lt(int id1, int id2) override;
    int if_eq(int id1, int id2) override;
    int if_neq(int id1, int id2) override;
    int if_gt(int id1, std::string_view constant) override;
    int if_lt(int id1, std::string_view constant) override;
    int if_eq(int id1, std::string_view constant) override;
    int if_neq(int id1, std::string_view constant) override;

//Branch operation
    int create_label() override;
    void insert_label(int label_id) override;
    void branch_return(int exp_id) override;
    void branch_return(std::string_view constant) override;
    void fn_return(int exp_id) override;
    void fn_return(std::string_view constant) override;
    void branch(int label_id) override;
    void branch_if_z(int expr_id, int label_id) override;
    void branch_if_nz(int expr_id, int label_id) override;

    void generate_code() override;

    std::string_view fetch_fn_name() const {
        return fn_name;
    }
};
