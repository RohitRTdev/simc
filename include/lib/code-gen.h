#pragma once
#include <string>
#include <vector>
#include <variant>
#include "spdlog/fmt/fmt.h"
#include "lib/dll.h"

#define INSTRUCTION( msg, ... ) fmt::format("\t" msg "\n" __VA_OPT__(,) __VA_ARGS__)

enum c_type {
    C_INT, 
    C_CHAR,
    C_LONGINT
};

struct c_var {
    std::string name;
    c_type type;
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
    virtual int declare_local_variable(const std::string& name, c_type type) = 0;

//Assign variable
    virtual void assign_var_int(int var_id, int id) = 0; 
    virtual void assign_var_int_c(int var_id, std::string_view constant) = 0; 
    virtual void assign_to_mem_int(int id1, int id2) = 0;
    virtual void assign_to_mem_int_c(int id, std::string_view constant) = 0;
    virtual int fetch_global_var_int(int id) = 0;
    virtual void assign_global_var_int(int id, int expr_id) = 0;
    virtual void assign_global_var_int_c(int id, std::string_view constant) = 0;

//Addition operation
    virtual int add_int(int id1, int id2) = 0; 
    virtual int add_int_c(int id, std::string_view constant) = 0; 

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
