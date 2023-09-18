#include "lib/x64/x64.h"

int x64_func::if_common(int id1, std::variant<int, std::string_view> object, compare_op op) {
    static constexpr const char* op_name[4] = {"if_gt", "if_lt", "if_eq", "if_neq"};
    insert_comment(op_name[op]);
    static constexpr const char* op_suffix[4] = {"g", "l", "e", "ne"};
    auto [reg1, _, type]  = unary_op_fetch(id1);
    bool is_signed = sign_list[reg1];

    if(std::holds_alternative<int>(object)) {
        int id2 = std::get<0>(object);
        reg_no_clobber_list[reg1] = true;
        int reg2 = fetch_result(id2);
        
        insert_code("cmp{} %{}, %{}", type, reg2, reg1);
        reg_no_clobber_list[reg1] = false;
        free_reg(reg2);
    }
    else {
        insert_code("cmp{} ${}, %{}", type, std::get<1>(object), reg1);
    }
    add_inst_to_code(INSTRUCTION("set{} %{}", op_suffix[op], get_register_string(C_CHAR, reg1)));
    set_reg_type(reg1, C_CHAR, false); //We consider bool type to be unsigned char(at code gen level atleast)
    int res_id = reg_status_list[reg1];
    if(type != C_CHAR || is_signed != false) {
        res_id = type_cast(reg_status_list[reg1], type, is_signed);
    }

    return res_id;
}

int x64_func::if_gt(int id1, int id2) {
    return if_common(id1, id2, GT);
}

int x64_func::if_gt(int id1, std::string_view constant) {
    return if_common(id1, constant, GT);
}

int x64_func::if_lt(int id1, int id2) {
    return if_common(id1, id2, LT);
}

int x64_func::if_lt(int id1, std::string_view constant) {
    return if_common(id1, constant, LT);
}

int x64_func::if_eq(int id1, int id2) {
    return if_common(id1, id2, EQ);
}

int x64_func::if_eq(int id1, std::string_view constant) {
    return if_common(id1, constant, EQ);
}

int x64_func::if_neq(int id1, int id2) {
    return if_common(id1, id2, NEQ);
}

int x64_func::if_neq(int id1, std::string_view constant) {
    return if_common(id1, constant, NEQ);
}
