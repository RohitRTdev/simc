#pragma once

#include <stack>
#include "compiler/ast.h"
#include "compiler/scope.h"
#include "compiler/type.h"
#include "debug-api.h"

enum class l_val_cat {
    LOCAL,
    GLOBAL,
    INDIR
};

struct expr_result {
    int expr_id;
    type_spec type;
    const token* var_token;
    bool is_lvalue;
    l_val_cat category;
    bool is_constant;
    std::string_view constant;

    expr_result() = default;

    expr_result(int id, type_spec new_type) : expr_result(new_type)
    {
        expr_id = id;
    }
    expr_result(type_spec new_type) : type(new_type)
    {
        expr_id = 0;
        var_token = nullptr;
        is_lvalue = false;
        is_constant = false;
    }


    expr_result(int id, type_spec m_type, const token* var, bool lval, l_val_cat cat, bool is_con = false, std::string_view con = std::string_view())
    : expr_id(id), type(m_type), var_token(var), is_lvalue(lval), category(cat), is_constant(is_con), constant(con)
    {}

    void convert_type(const expr_result& res2, Ifunc_translation* fn_intf) {
        expr_id = type_spec::convert_type(res2.type, expr_id, type, fn_intf, !is_constant);
    }

    void convert_to_integral_type(Ifunc_translation* fn_intf) {
        if(type.is_void()) {
            sim_log_error("void type cannot be converted to integral type");
        }
        if(type.is_array_type() || type.is_function_type()) {
            type.convert_to_pointer_type();
        }
        if(type.is_pointer_type()) {
            type_spec int_type{};
            int_type.base_type = C_INT;
            int_type.is_signed = true;
            expr_id = type_spec::convert_type(int_type, expr_id, type, fn_intf, !is_constant);
        }
    }

    void convert_to_ptrdiff(Ifunc_translation* fn_intf) {
        CRITICAL_ASSERT(type.is_pointer_type(), "convert_to_ptrdiff() called on non pointer type");
        type_spec final_type{};
        final_type.base_type = C_LONGLONG;
        final_type.is_signed = true;
    
        //Convert to a signed long long
        expr_id = type_spec::convert_type(final_type, expr_id, type, fn_intf);
    }

    void free(Ifunc_translation* fn) {
        if(!is_constant && expr_id) {
            fn->free_result(expr_id);
        }
    }
};

class eval_expr {
    std::stack<expr_result> res_stack;
    std::stack<std::unique_ptr<ast>> op_stack;
    std::stack<std::tuple<std::string_view, size_t, size_t, bool>> fn_call_stack;
    std::stack<std::tuple<int, int>> logical_stack;

    bool is_assignable() const;
    bool is_base_equal(const type_spec& type_1, const type_spec& type_2);
    bool is_rank_same(const type_spec& type_1, const type_spec& type_2);
    bool is_rank_higher(const type_spec& type_1, const type_spec& type_2);
    void perform_arithmetic_conversion(expr_result& res1, expr_result& res2);
    void perform_integer_promotion(expr_result& res1);
    void convert_type(expr_result& res1, expr_result& res2);

    scope* fn_scope;
    Ifunc_translation* fn_intf;
    std::unique_ptr<ast> expr_node;
    const ast_expr* expr;
    static std::vector<std::string> const_storage;
    bool hook();

    void main_loop();
    void handle_node();
    void handle_var();
    void handle_constant();
    void handle_arithmetic_op(operator_type op);
    void handle_assignment();
    void handle_comma_expr();
    void handle_unary_op(operator_type op);
    void handle_indir();
    void handle_inc_dec(operator_type op, bool is_postfix);
    void handle_addr();
    void handle_fn_call();
    void handle_simple_unary_op(operator_type op);
    bool handle_pointer_arithmetic(expr_result& res1, expr_result& res2, operator_type op);
    void handle_logical_op(operator_type op);


public:
    eval_expr(std::unique_ptr<ast> expr_start, Ifunc_translation* fn, scope* cur_scope);
    expr_result eval();
};