#include "lib/x64/x64.h"

int x64_func::create_label() {
    return new_label_id++;
}

void x64_func::add_label(int label_id) {
    add_inst_to_code(fmt::format(LINE(".L{}:"), label_id));
}

void x64_func::branch_return(int exp_id) {
    if(!ret_label_id)
        ret_label_id = new_label_id++;
    transfer_to_reg(RAX, exp_id);
    add_inst_to_code(INSTRUCTION("jmp .L{}", ret_label_id));
}

void x64_func::fn_return(int exp_id) {
    auto [type, is_signed] = fetch_result_type(exp_id);
    CRITICAL_ASSERT(type == m_ret_type && is_signed == m_is_signed, "Return type mismatch during fn_return() call");
    transfer_to_reg(RAX, exp_id);
    free_reg(RAX);
}

void x64_func::fn_return(std::string_view constant) {
    free_preferred_register(RAX, m_ret_type, m_is_signed);
    insert_code("mov{} ${}, %{}", m_ret_type, constant, RAX);
    free_reg(RAX);
}

