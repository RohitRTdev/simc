#pragma once
#include <string>
#include <vector>
#include "spdlog/fmt/fmt.h"
#include "dll.h"

#define INSTRUCTION( msg, ... ) fmt::format("\t" msg "\n" __VA_OPT__(,) __VA_ARGS__)

enum c_type {
    C_INT, 
    C_CHAR
};

struct c_var {
    std::string name;
    c_type type;
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
    virtual int declare_local_variable(const std::string& name, c_type type) = 0;

//Assign variable
    virtual void assign_ii(int var_id1, int var_id2) = 0;
    virtual void assign_ic(int var_id1, int constant) = 0;
    virtual void assign_ir(int var_id, int exp_id) = 0;
    virtual void assign_to_mem_i(int exp_id, int var_id) = 0;
    virtual void assign_to_mem_r(int exp_id1, int exp_id2) = 0;
    virtual void assign_to_mem_c(int exp_id, int constant) = 0;

//Addition operation
    virtual int add_ii(int var_id1, int var_id2) = 0;
    virtual int add_ic(int var_id, int constant) = 0;
    virtual int add_ri(int exp_id, int var_id) = 0;
    virtual int add_rc(int exp_id, int constant) = 0;
    virtual int add_rr(int exp_id1, int exp_id2) = 0;

    virtual void generate_code() = 0;

    virtual ~Ifunc_translation() = default;
};

class Itranslation : public Itrbase {
    std::string body;
public:
    virtual Ifunc_translation* add_function(std::string name, const std::vector<c_var>& decl_list, c_type ret_type);

};

DLL_ATTRIB Ifunc_translation* create_function_translation_unit(); 
