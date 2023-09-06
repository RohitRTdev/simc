#include <stack>
#include "compiler/ast.h"
#include "compiler/type.h"
#include "compiler/scope.h"
#include "compiler/eval.h"
#include "compiler/utils.h"

eval_expr::eval_expr(std::unique_ptr<ast> expr_start, Ifunc_translation* fn, scope* cur_scope, bool only_eval) : expr_node(std::move(expr_start)), 
fn_intf(fn), fn_scope(cur_scope), eval_only(only_eval) 
{}

int eval_expr::eval() {
    main_loop();

    CRITICAL_ASSERT(res_stack.size() == 1, "res_stack.size() == 1 assertion failed during call to eval()");
    return fetch_stack_node(res_stack).expr_id;
}

bool eval_expr::is_assignable() const {
    if(!op_stack.size())
        return false;

    auto _expr = cast_to_ast_expr(op_stack.top());
    if(_expr->is_operator() && cast_to_ast_op(op_stack.top())->tok->is_operator_eq()) {
        //This means we're looking at left child of EQUAL operator
        if(_expr->children.size() == 1) {
            return true;
        }
    }
    
    return false; 
}

bool eval_expr::is_base_equal(const type_spec& type_1, const type_spec& type_2) {
    return type_1.base_type == type_2.base_type && type_1.is_signed == type_2.is_signed;
}

bool eval_expr::is_rank_same(const type_spec& type_1, const type_spec& type_2) {
    return type_1.base_type == type_2.base_type;
}

bool eval_expr::is_rank_higher(const type_spec& type_1, const type_spec& type_2) {
    return type_1.base_type > type_2.base_type;
}

void eval_expr::convert_type(expr_result& res1, expr_result& res2) {
    sim_log_debug("Converting type:{} with sign:{} to type:{} with sign:{}",
    res1.type.base_type, res1.type.is_signed,
    res2.type.base_type, res2.type.is_signed);

    if(!res1.is_constant) {
        res1.expr_id = fn_intf->type_cast(res1.expr_id, res2.type.base_type, res2.type.is_signed);
    }

    res1.type.base_type = res2.type.base_type;
    res1.type.is_signed = res2.type.is_signed;
}

void eval_expr::perform_integer_promotion(expr_result& res) {    
    if(res.type.base_type < C_INT) {
        sim_log_debug("Performing integer promotion from type:{} to int", res.type.base_type);
        if(!res.is_constant)
            res.expr_id = fn_intf->type_cast(res.expr_id, C_INT, true);
        res.type.base_type = C_INT;
        res.type.is_signed = true;
    }
}

void eval_expr::perform_arithmetic_conversion(expr_result& res1, expr_result& res2) {
    perform_integer_promotion(res1);
    perform_integer_promotion(res2);

    if(is_base_equal(res1.type, res2.type)) {
        return;
    }

    if(res1.type.is_signed == res2.type.is_signed) {
        if(is_rank_higher(res1.type, res2.type)) {
            convert_type(res2, res1);
        }
        else {
            convert_type(res1, res2);
        }
    }
    else {
        auto& res_u = res1.type.is_signed ? res2 : res1;
        auto& res_s = res2.type.is_signed ? res2 : res1;
        if(is_rank_higher(res_u.type, res_s.type) || is_rank_same(res_u.type, res_s.type)) {
            convert_type(res_s, res_u);
        }
        else {
            convert_type(res_u, res_s);
        }
    }
}

void eval_expr::handle_arithmetic_op(operator_type op) {
    
    int (Ifunc_translation::*op_vars)(int, int);
    int (Ifunc_translation::*op_var_con)(int, std::string_view);
    int (Ifunc_translation::*op_con_var)(std::string_view, int);
    
    auto res2 = fetch_stack_node(res_stack);
    auto res1 = fetch_stack_node(res_stack);

    if(!res1.type.is_type_operable(res2.type)) {
        if(res1.type.is_modified_type() || res1.type.is_modified_type()) {
            sim_log_error("Arithmetic operation requires both types to be integral and equal");
        }
    }

    perform_arithmetic_conversion(res1, res2);

    bool is_commutative = true;
    int res_id = 0;
    switch(op) {
        case PLUS: {
            op_vars = &Ifunc_translation::add; op_var_con = &Ifunc_translation::add;
            break;
        }
        case MINUS: {
            op_vars = &Ifunc_translation::sub; op_var_con = &Ifunc_translation::sub;
            op_con_var = &Ifunc_translation::sub;
            is_commutative = false;
            break;
        }
        case MUL: {
            op_vars = &Ifunc_translation::mul; op_var_con = &Ifunc_translation::mul;
            break;
        }
        default: {
            CRITICAL_ASSERT_NOW("handle_arithmetic_op called with invalid operator");
        }
    }

    if(!res1.is_constant && !res2.is_constant) {
        res_id = call_code_gen(fn_intf, op_vars, res1.expr_id, res2.expr_id);
    }
    else if(res1.is_constant && res2.is_constant) {
        CRITICAL_ASSERT_NOW("Constant folding feature is not supported yet");
    }
    else {
        if(res1.is_constant) {
            if(!is_commutative) {
                res_id = call_code_gen(fn_intf, op_con_var, res1.constant, res2.expr_id);
            }
            else {
                res_id = call_code_gen(fn_intf, op_var_con, res2.expr_id, res1.constant);
            }
        }
        else {
            res_id = call_code_gen(fn_intf, op_var_con, res1.expr_id, res2.constant);
        }
    }

    expr_result res{};
    res.expr_id = res_id;
    res.type = res1.type;

    res_stack.push(res);
}

void eval_expr::handle_assignment() {
    auto res2 = fetch_stack_node(res_stack);
    auto res1 = fetch_stack_node(res_stack);

    if(!res1.is_lvalue || !res1.type.is_modifiable()) {
        sim_log_error("LHS of '=' operator must be a modifiable l-value");
    }

    if(res1.type.is_pointer_type()) {
        if(!(res2.type.is_pointer_type() && (res1.type.resolve_type() == res2.type.resolve_type()))) {
            sim_log_error("Pointer type mismatch during assignment operation");
        }
    }

    if(!(res1.type == res2.type)) {
        convert_type(res2, res1);
    }

    int (Ifunc_translation::*assign_con)(int, std::string_view);
    int (Ifunc_translation::*assign_var)(int, int);

    int res_id = 0;
    switch(res1.category) {
        case l_val_cat::LOCAL: {
            if(res2.is_constant) {
                assign_con = &Ifunc_translation::assign_var;
                res_id = call_code_gen(fn_intf, assign_con, res1.expr_id, res2.constant);
            } 
            else {
                assign_var = &Ifunc_translation::assign_var;
                res_id = call_code_gen(fn_intf, assign_var, res1.expr_id, res2.expr_id);
            }
            break;
        }
        case l_val_cat::GLOBAL: {
            if(res2.is_constant) {
                assign_con = &Ifunc_translation::assign_global_var;
                res_id = call_code_gen(fn_intf, assign_con, res1.expr_id, res2.constant); 
            }
            else {
                assign_var = &Ifunc_translation::assign_global_var;
                res_id = call_code_gen(fn_intf, assign_var, res1.expr_id, res2.expr_id); 
            }
            break;
        }
        default: CRITICAL_ASSERT_NOW("Invalid l-value category encountered during assignment operation");
    }

    res1.is_lvalue = false;
    res1.expr_id = res_id;
    res_stack.push(res1);
}

void eval_expr::handle_var() {
    auto& var = fn_scope->fetch_var_info(std::get<std::string>(expr->tok->value));

    int var_id = var.var_id;
    auto category = l_val_cat::LOCAL;
    if(var.is_global) {
        if(!is_assignable()) {
            var_id = fn_intf->fetch_global_var(var.var_id);
        }
        category = l_val_cat::GLOBAL;
    }
    expr_result res{var_id, var.type, expr->tok, true, category};
    res_stack.push(res);
}

void eval_expr::handle_constant() {
    //We consider a constant by default to be a signed integer
    type_spec type{};
    type.base_type = C_INT;
    type.is_signed = true;
    
    expr_result res{};
    res.is_constant = true;
    res.constant = std::get<std::string>(expr->tok->value);
    res.type = type;

    res_stack.push(res);
}

void eval_expr::handle_node() {
    expr = cast_to_ast_expr(expr_node);
    if(expr->is_var()) {
        handle_var();
    }
    else if(expr->is_con()) {
        handle_constant();
    }
    else if(expr->is_operator()) {
        auto op = std::get<operator_type>(cast_to_ast_op(expr_node)->tok->value);
        
        switch(op) {
            case PLUS:
            case MINUS:
            case MUL: {
                handle_arithmetic_op(op);
                break;
            }
            case EQUAL: {
                handle_assignment();
                break;
            }
            default: CRITICAL_ASSERT_NOW("Only +, - and = operators supported right now");
        }
    }
    else {
        CRITICAL_ASSERT_NOW("Invalid expr_node encountered during handle_node evaluation");
    }

    expr_node.reset();
}

void eval_expr::main_loop() {
    while(!op_stack.empty() || expr_node) {
        if(!expr_node) {
            expr_node = fetch_stack_node(op_stack);
        }
        else {
            if (expr_node->children.size()) {
                auto child = fetch_child(expr_node);
                op_stack.push(std::move(expr_node));
                expr_node = std::move(child);
            }
            else {
                handle_node();
            }
        }
    }
}