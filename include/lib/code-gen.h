#pragma once
#include <string>
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
    C_INT, 
    C_CHAR,
    C_LONG,
    C_SHORT,
    C_LONGLONG,

    //These types are used since they are needed in certain situations, however they are not
    //used physically while moving data from one register to another in a given arch 
    C_VOID,
    C_PTR
};

#define NUM_TYPES 5

struct c_var {
    std::string name;
    c_type type;
    bool is_signed;
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
    virtual int declare_local_variable(const std::string& name, c_type type, bool is_signed) = 0;
    virtual void free_result(int exp_id) = 0;

//Assign variable
    virtual int assign_var(int var_id, int id) = 0; 
    virtual int assign_var(int var_id, std::string_view constant) = 0; 
    virtual int assign_to_mem(int id1, int id2) = 0;
    virtual int assign_to_mem(int id, std::string_view constant, c_type type) = 0;
    virtual int fetch_global_var(int id) = 0;
    virtual int assign_global_var(int id, int expr_id) = 0;
    virtual int assign_global_var(int id, std::string_view constant) = 0;

//Addition operation
    virtual int add(int id1, int id2) = 0; 
    virtual int add(int id, std::string_view constant) = 0; 
    virtual int sub(int id1, int id2) = 0;
    virtual int sub(int id, std::string_view constant) = 0;
    virtual int sub(std::string_view constant, int id) = 0;

//Branch operation
    virtual int create_label() = 0;
    virtual void add_label(int label_id) = 0;
    virtual void branch_return(int exp_id) = 0;
    virtual void fn_return(int exp_id) = 0;

    virtual void generate_code() = 0;

    virtual ~Ifunc_translation() = default;
};

class Itranslation : public Itrbase {
public:
    virtual Ifunc_translation* add_function(const std::string& name) = 0;
    virtual int declare_global_variable(const std::string& name, c_type type) = 0;
    virtual int declare_global_variable(const std::string& name, c_type type, std::string_view constant) = 0;
    virtual void generate_code() = 0;

    virtual ~Itranslation() = default;
};

DLL_ATTRIB Itranslation* create_translation_unit(); 
