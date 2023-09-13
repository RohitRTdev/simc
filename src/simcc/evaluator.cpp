#include <stack>
#include "compiler/ast.h"
#include "compiler/type.h"
#include "compiler/scope.h"
#include "compiler/eval.h"
#include "compiler/utils.h"

std::vector<std::string> eval_expr::const_storage;

eval_expr::eval_expr(std::unique_ptr<ast> expr_start, Ifunc_translation* fn, scope* cur_scope) : expr_node(std::move(expr_start)), 
fn_intf(fn), fn_scope(cur_scope) 
{
    const_storage.clear();
}

expr_result eval_expr::eval() {
    main_loop();

    CRITICAL_ASSERT(res_stack.size() == 1, "res_stack.size() == 1 assertion failed during call to eval()");
    return fetch_stack_node(res_stack);
}

bool eval_expr::is_assignable() const {
    if(!op_stack.size())
        return false;

    auto _expr = cast_to_ast_expr(op_stack.top());
    if(_expr->is_operator()) {
        auto op = cast_to_ast_op(op_stack.top());
        if(op->tok->is_operator_eq() && _expr->children.size() == 1) {
            return true;
        }
        else {
            auto sym = std::get<operator_type>(op->tok->value);
            switch(sym) {
                case INCREMENT:
                case DECREMENT: return true; 
                case AMPER: {
                    if(op->is_unary)
                        return true;
                }
            }
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


void eval_expr::perform_integer_promotion(expr_result& res) {    
    if(res.type.base_type < C_INT) {
        sim_log_debug("Performing integer promotion from type:{} to int", res.type.base_type);
        if(!res.is_constant)
            res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::type_cast, res.expr_id, C_INT, true);
        
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
            res2.convert_type(res1, fn_intf);
        }
        else {
            res1.convert_type(res2, fn_intf);
        }
    }
    else {
        auto& res_u = res1.type.is_signed ? res2 : res1;
        auto& res_s = res2.type.is_signed ? res2 : res1;
        if(is_rank_higher(res_u.type, res_s.type) || is_rank_same(res_u.type, res_s.type)) {
            res_s.convert_type(res_u, fn_intf);
        }
        else {
            res_u.convert_type(res_s, fn_intf);
        }
    }
}


bool eval_expr::handle_pointer_arithmetic(expr_result& res1, expr_result& res2, operator_type op) {
    expr_result& res_p = res1, &res_i = res2;
    if(res1.type.is_pointer_type() && res2.type.is_integral()) {
        res_p = res1;
        res_i = res2;
    }
    else if(res2.type.is_pointer_type() && res1.type.is_integral()) {
        res_p = res2;
        res_i = res1;
    }
    else {
        return false;
    }

    if(op != PLUS && op != MINUS) {
        return false;
    }

    if(res_p.type.is_incomplete_type()) {
        sim_log_error("Pointer arithmetic cannot be carried out on a pointer to an incomplete type");
    }

    sim_log_debug("Performing pointer arithmetic");
    CRITICAL_ASSERT(!res_i.is_constant, "Pointer arithmetic with constant not yet supported");
    res_i.convert_type(res_p, fn_intf);

    std::string inc_count = std::to_string(res_p.type.get_pointer_base_type_size());
    int res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::mul, res_i.expr_id, std::string_view(inc_count));
    
    int (Ifunc_translation::*op_var)(int, int); 
    if(op == PLUS)
        op_var = &Ifunc_translation::add;
    else
        op_var = &Ifunc_translation::sub;

    res_id = code_gen::call_code_gen(fn_intf, op_var, res_p.expr_id, res_id);

    expr_result res{res_p.type};
    res.expr_id = res_id;

    res_stack.push(res);
    return true;
}

void eval_expr::handle_arithmetic_op(operator_type op) {
    
    int (Ifunc_translation::*op_vars)(int, int);
    int (Ifunc_translation::*op_var_con)(int, std::string_view);
    int (Ifunc_translation::*op_con_var)(std::string_view, int);
    
    auto res2 = fetch_stack_node(res_stack);
    auto res1 = fetch_stack_node(res_stack);

    auto convert_to_ptr_type = [&] (expr_result& res) {
        if(res.type.is_array_type() || res.type.is_function_type()) {
            res.type.convert_to_pointer_type();
        }
    };

    //Convert to pointer type
    convert_to_ptr_type(res1);
    convert_to_ptr_type(res2);

    if(handle_pointer_arithmetic(res1, res2, op))
        return;


    if(res1.type.is_pointer_type() && res2.type.is_pointer_type()) {
        if(op != MINUS) {
            sim_log_error("Arithmetic operation can have 2 pointer operands only if it is a subtraction");
        }
        res1.convert_to_ptrdiff(fn_intf);
        res2.convert_to_ptrdiff(fn_intf);

        int res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::sub, res1.expr_id, res2.expr_id);
        expr_result res{res_id, res1.type};
        res_stack.push(res);
        return;
    }

    if(!res1.type.is_type_operable(res2.type)) {
        sim_log_error("Arithmetic operation requires both operands to be of arithmetic types");
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
        res_id = code_gen::call_code_gen(fn_intf, op_vars, res1.expr_id, res2.expr_id);
    }
    else if(res1.is_constant && res2.is_constant) {
        CRITICAL_ASSERT_NOW("Constant folding feature is not supported yet");
    }
    else {
        if(res1.is_constant) {
            if(!is_commutative) {
                res_id = code_gen::call_code_gen(fn_intf, op_con_var, res1.constant, res2.expr_id);
            }
            else {
                res_id = code_gen::call_code_gen(fn_intf, op_var_con, res2.expr_id, res1.constant);
            }
        }
        else {
            res_id = code_gen::call_code_gen(fn_intf, op_var_con, res1.expr_id, res2.constant);
        }
    }

    expr_result res{res_id, res1.type};
    res_stack.push(res);
}

void eval_expr::handle_assignment() {
    auto res2 = fetch_stack_node(res_stack);
    auto res1 = fetch_stack_node(res_stack);

    if(!res1.is_lvalue || !res1.type.is_modifiable()) {
        sim_log_error("LHS of '=' operator must be a modifiable l-value");
    }

    if(!res1.type.is_type_convertible(res2.type)) {
        sim_log_error("Incompatible types encountered during assignment operation");
    }

    res2.convert_type(res1, fn_intf);

    int res_id = 0;
    switch(res1.category) {
        case l_val_cat::LOCAL: {
            if(res2.is_constant) {
                res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::assign_var, res1.expr_id, res2.constant);
            } 
            else {
                res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::assign_var, res1.expr_id, res2.expr_id);
            }
            break;
        }
        case l_val_cat::GLOBAL: {
            if(res2.is_constant) {
                res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::assign_global_var, res1.expr_id, res2.constant); 
            }
            else {
                res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::assign_global_var, res1.expr_id, res2.expr_id); 
            }
            break;
        }
        case l_val_cat::INDIR: {
            auto [base_type, _] = res2.type.get_simple_type();
            if(res2.is_constant) {
                res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::assign_to_mem, res1.expr_id, res2.constant, base_type);
            }            
            else {
                res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::assign_to_mem, res1.expr_id, res2.expr_id); 
            }
            break;
        }
        default: CRITICAL_ASSERT_NOW("Invalid l-value category encountered during assignment operation");
    }

    res2.is_lvalue = false;
    res2.expr_id = res_id;
    res_stack.push(res2);
}


void eval_expr::handle_addr() {
    auto in = fetch_stack_node(res_stack);   

    if(!in.is_lvalue) {
        sim_log_error("'&' operator requires operand to be an lvalue");
    }

    bool is_mem = in.category == l_val_cat::INDIR;
    bool is_global = in.category == l_val_cat::GLOBAL;

    expr_result res{in.type.addr_type()};

    if(in.type.is_function_type()) {
        std::string_view fn_name = std::get<std::string>(in.var_token->value);
        res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::get_address_of, fn_name);
    }
    else {
        res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::get_address_of, in.expr_id, is_mem, is_global);
    }

    res_stack.push(res);
}

void eval_expr::handle_indir() {
    auto res_in = fetch_stack_node(res_stack);

    //Must be a pointer
    if(!res_in.type.is_pointer_type()) {
        if(res_in.type.is_convertible_to_pointer_type()) {
            res_in.type.convert_to_pointer_type();
        }
        else {
            sim_log_error("'*' operator requires operand to be a pointer type");
        }
    }

    expr_result res{res_in.type.resolve_type()};
    auto [base_type, is_signed] = res.type.get_simple_type();
    res.expr_id = res_in.expr_id;

    if(!is_assignable()) {
        res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::fetch_from_mem, res_in.expr_id, base_type, is_signed); 
    }

    res.category = l_val_cat::INDIR;
    res.is_lvalue = true;
    res_stack.push(res);
}

void eval_expr::handle_inc_dec(operator_type op, bool is_postfix) {
    auto in = fetch_stack_node(res_stack);
    if(!in.is_lvalue || !in.type.is_modifiable()) {
        sim_log_error("Operand to a inc/dec operator must be a modifiable lvalue");
    }

    if(in.type.is_incomplete_type()) {
        sim_log_error("inc/dec operator cannot be applied to an incomplete type");
    }

    expr_result res{in.type};
    auto [base_type, is_signed] = res.type.get_simple_type();
    size_t inc_count = res.type.is_pointer_type() ? res.type.get_pointer_base_type_size() : 1; 
    bool is_mem = in.category == l_val_cat::INDIR; //If dereference or array subscript, force memory variable case
    bool is_global = in.category == l_val_cat::GLOBAL;
    auto ptr_mem = &Ifunc_translation::post_inc;
    switch(op) {
        case INCREMENT: {
            if(is_postfix) {
                ptr_mem = &Ifunc_translation::post_inc;
            }
            else {
                ptr_mem = &Ifunc_translation::pre_inc;
            }
            break;
        }
        case DECREMENT: {
            if(is_postfix) {
                ptr_mem = &Ifunc_translation::post_dec;
            }
            else {
                ptr_mem = &Ifunc_translation::pre_dec;
            }
            break;
        }
        default: {
            CRITICAL_ASSERT_NOW("Invalid operator passed to handle_inc_dec()");
        }
    }

    res.expr_id = code_gen::call_code_gen(fn_intf, ptr_mem, in.expr_id, base_type, is_signed, inc_count, is_mem, is_global);
    res_stack.push(res);
}

void eval_expr::handle_unary_op(operator_type op) {
    switch(op) {
        case MUL: {
            handle_indir();
            break;
        }
        case AMPER: {
            handle_addr();
            break;
        }
        case INCREMENT:  
        case DECREMENT: { 
            handle_inc_dec(op, false);
            break;
        }
        default: CRITICAL_ASSERT_NOW("handle_unary_op() called with invalid operator_type");
    } 
}

void eval_expr::handle_var() {
    auto& var = fn_scope->fetch_var_info(std::get<std::string>(expr->tok->value));

    int var_id = var.var_id;
    auto category = l_val_cat::LOCAL;
    if(var.type.is_function_type()) {
        if(!is_assignable()) {
            var_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::get_address_of, var.name);
        }
        if(var.is_global) {
            category = l_val_cat::GLOBAL;
        }
    }
    else if(var.is_global) {
        if(!is_assignable()) {
            var_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::fetch_global_var, var.var_id);
        }
        category = l_val_cat::GLOBAL;
    }
    expr_result res{var_id, var.type, expr->tok, true, category};
    res_stack.push(res);
}

void eval_expr::handle_constant() {
    //We consider a constant by default to be a signed integer
    CRITICAL_ASSERT(!expr->tok->is_string_constant(), "String constant not supported right now");

    type_spec type{};
    type.base_type = expr->tok->is_integer_constant() ? C_INT : C_CHAR;
    type.is_signed = true;

    expr_result res{};
    res.is_constant = true;
    if(type.base_type == C_CHAR) {
        const_storage.push_back(std::to_string(int(std::get<char>(expr->tok->value))));
        res.constant = const_storage.back();
    }
    else 
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
        auto op = cast_to_ast_op(expr_node);
        auto sym = std::get<operator_type>(op->tok->value);

        if(op->is_postfix) {
            handle_inc_dec(sym, true);
        }
        else if(op->is_unary) {
            handle_unary_op(sym);
        }
        else {
            switch(sym) {
                case EQUAL: {
                    handle_assignment();
                    break;
                }
                default: {
                    handle_arithmetic_op(sym);
                }
            }
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