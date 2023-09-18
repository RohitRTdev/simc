#pragma once
#include <string>
#include <optional>
#include <vector>
#include <variant>
#include "spdlog/fmt/fmt.h"
#include "lib/dll.h"

#ifdef ARCH_X64
#include "lib/x64/x64_types.h"
#else
#error "Only x64 architecture currently supported"
#endif

#define LINE(msg) msg "\n"
#define INSTRUCTION( msg, ... ) fmt::format("\t" LINE(msg) __VA_OPT__(,) __VA_ARGS__)


enum c_type {
    C_CHAR,
    C_SHORT,
    C_INT, 
    C_LONG,
    C_LONGLONG,

    //These types are used since they are needed in certain situations, however they are not
    //used physically while moving data from one register to another in a given arch 
    C_VOID,
    C_PTR
};


static inline constexpr size_t base_type_size(c_type type) {
    switch (type) {
    case C_INT: return C_INTSIZE;
    case C_CHAR: return C_CHARSIZE;
    case C_SHORT: return C_SHORTSIZE;
    case C_LONG: return C_LONGSIZE;
    case C_LONGLONG: return C_LONGLONGSIZE;
    case C_PTR: return C_PTRSIZE;
    }

    return 0; //Control should never reach here
}

#define NUM_TYPES 5
//Code generator's view of a variable
struct c_var {
    std::string name;
    c_type type;
    std::optional<size_t> mem_var_size;
    bool is_signed;
    bool is_static;
    bool is_fn;
    std::string value;
};

class Itrbase {
protected:
    std::string code;
    void add_inst_to_code(const std::string& inst) {
        code += inst;
    }

public:
    std::string fetch_code() {
        return code;
    }
};

class Ifunc_translation : public Itrbase {
public:
//Declaration
    virtual int declare_local_variable(std::string_view name, c_type type, bool is_signed) = 0;
    virtual int declare_local_mem_variable(std::string_view name, size_t mem_var_size) = 0; 
    virtual void free_result(int exp_id) = 0;
    virtual int create_temporary_value(c_type type, bool is_signed) = 0;
    virtual int set_value(int expr_id, std::string_view constant) = 0;

//function
    virtual int save_param(std::string_view name, c_type type, bool is_signed) = 0;
    virtual void begin_call_frame(size_t num_args) = 0;
    virtual void load_param(int exp_id) = 0;
    virtual void load_param(std::string_view constant, c_type type, bool is_signed) = 0;
    virtual int call_function(int exp_id, c_type ret_type, bool is_signed) = 0;
    virtual int call_function(std::string_view name, c_type ret_type, bool is_signed) = 0;

//Assign variable
    virtual int assign_var(int var_id, int id) = 0; 
    virtual int assign_var(int var_id, std::string_view constant) = 0; 
    virtual int assign_to_mem(int id1, int id2) = 0;
    virtual int assign_to_mem(int id, std::string_view constant, c_type type) = 0;
    virtual int fetch_from_mem(int id, c_type type, bool is_signed) = 0;
    virtual int fetch_from_mem(std::string_view constant, c_type type, bool is_signed) = 0;
    virtual int fetch_global_var(int id) = 0;
    virtual int assign_global_var(int id, int expr_id) = 0;
    virtual int assign_global_var(int id, std::string_view constant) = 0;
    virtual int get_address_of(std::string_view constant) = 0;
    virtual int get_address_of(int id, bool is_mem, bool is_global) = 0;

//Addition operation
    virtual int add(int id1, int id2) = 0; 
    virtual int add(int id, std::string_view constant) = 0; 
    virtual int sub(int id1, int id2) = 0;
    virtual int sub(int id, std::string_view constant) = 0;
    virtual int sub(std::string_view constant, int id) = 0;
    virtual int mul(int id1, int id2) = 0;
    virtual int mul(int id1, std::string_view constant) = 0;
    virtual int div(int id1, std::string_view constant) = 0;
    virtual int div(int id1, int id2) = 0;
    virtual int div(std::string_view constant, int id) = 0;
    virtual int modulo(int id1, int id2) = 0;
    virtual int modulo(int id1, std::string_view constant) = 0;
    virtual int modulo(std::string_view constant, int id) = 0;
    virtual int bit_and(int id1, int id2) = 0;
    virtual int bit_and(int id1, std::string_view constant) = 0;
    virtual int bit_or(int id1, int id2) = 0;
    virtual int bit_or(int id1, std::string_view constant) = 0;
    virtual int bit_xor(int id1, int id2) = 0;
    virtual int bit_xor(int id1, std::string_view constant) = 0;
    virtual int bit_not(int id) = 0;
    virtual int pre_inc(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) = 0;
    virtual int pre_dec(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) = 0;
    virtual int post_inc(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) = 0;
    virtual int post_dec(int id, c_type type, bool is_signed, size_t inc_count, bool is_mem, bool is_global) = 0;
    virtual int negate(int id) = 0;
    
//Type conversion
    virtual int type_cast(int exp_id, c_type cast_type, bool cast_sign) = 0;

//Conditional operation
    virtual int if_gt(int id1, int id2) = 0;
    virtual int if_lt(int id1, int id2) = 0;
    virtual int if_eq(int id1, int id2) = 0;
    virtual int if_neq(int id1, int id2) = 0;
    virtual int if_gt(int id1, std::string_view constant) = 0;
    virtual int if_lt(int id1, std::string_view constant) = 0;
    virtual int if_eq(int id1, std::string_view constant) = 0;
    virtual int if_neq(int id1, std::string_view constant) = 0;

//Branch operation
    virtual int create_label() = 0;
    virtual void insert_label(int label_id) = 0;
    virtual void branch_return(int exp_id) = 0;
    virtual void branch_return(std::string_view constant) = 0;
    virtual void fn_return(int exp_id) = 0;
    virtual void fn_return(std::string_view constant) = 0;
    virtual void branch(int label_id) = 0;
    virtual void branch_if_z(int expr_id, int label_id) = 0;
    virtual void branch_if_nz(int expr_id, int label_id) = 0;

    virtual void generate_code() = 0;

    virtual ~Ifunc_translation() = default;
};

class Itranslation : public Itrbase {
public:
    virtual Ifunc_translation* add_function(std::string_view name, c_type ret_type, bool is_signed, bool is_static) = 0; 
    virtual int declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static) = 0;
    virtual int declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static, std::string_view constant) = 0;
    virtual int declare_global_mem_variable(std::string_view name, bool is_static, size_t mem_var_size) = 0; 
    virtual void generate_code() = 0;

    virtual ~Itranslation() = default;
};

DLL_ATTRIB Itranslation* create_translation_unit(); 
