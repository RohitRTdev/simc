#pragma once
#include <string>
#include <vector>

enum c_type {
    INT, 
    CHAR
};

struct c_var {
    std::string name;
    c_type type;
};

class Ifunc_translation {
    std::string code;
public:

//Assign variable
    virtual int assign_ii(int var_id1, int var_id2);
    virtual int assign_ic(int var_id1, int constant);
    virtual int assign_ir(int var_id, int exp_id);

//Addition operation
    virtual int add_ii(int var_id1, int var_id2);
    virtual int add_ic(int var_id, int constant);
    virtual int add_ri(int exp_id, int var_id);
    virtual int add_rc(int exp_id, int constant);

};

class Itranslation {
    std::string body;
public:
    virtual Ifunc_translation* add_function(std::string name, const std::vector<c_var>& decl_list, c_type ret_type);

};


Itranslation* create_translation_unit();

