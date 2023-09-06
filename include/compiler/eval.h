#pragma once

#include <stack>
#include "compiler/ast.h"
#include "compiler/scope.h"
#include "compiler/type.h"

class eval_expr {
    bool eval_only;

    //Template conundrum to allow only type T and U where U is a pointer to member function of T which takes 
    //arbitrary number of arguments and returns int.
    template<typename T, typename U, typename... Args,
        typename = typename std::enable_if<std::is_same<int, decltype((std::declval<T>()->*U())(std::declval<Args>()...))>::value>::type>
        int call_code_gen(T intf, U member, const Args&... args) {
        if (!eval_only) {
            //We don't need to forward arguments here as it doesn't make any difference
            return (intf->*member)(args...);
        }
        return 0;
    }

    enum class l_val_cat {
        LOCAL,
        GLOBAL
    };

    struct expr_result {
        int expr_id;
        type_spec type;
        const token* var_token;
        bool is_lvalue;
        l_val_cat category;
        bool is_constant;
        std::string constant;
    };

    std::stack<expr_result> res_stack;
    std::stack<std::unique_ptr<ast>> op_stack;

    using common_type_res = std::pair<int, int>;

    bool is_assignable() const;
    bool is_base_equal(const type_spec& type_1, const type_spec& type_2);
    bool is_rank_same(const type_spec& type_1, const type_spec& type_2);
    bool is_rank_higher(const type_spec& type_1, const type_spec& type_2);
    void convert_type(expr_result& res1, expr_result& res2);
    void perform_arithmetic_conversion(expr_result& res1, expr_result& res2);
    void perform_integer_promotion(expr_result& res1);
    scope* fn_scope;
    Ifunc_translation* fn_intf;
    std::unique_ptr<ast> expr_node;
    const ast_expr* expr;

    void main_loop();
    void handle_node();
    void handle_var();
    void handle_constant();
    void handle_arithmetic_op(operator_type op);
    void handle_assignment();

public:
    eval_expr(std::unique_ptr<ast> expr_start, Ifunc_translation* fn, scope* cur_scope, bool only_eval);
    int eval();
};