#include <stack>
#include "compiler/ast.h"
#include "compiler/type.h"
#include "compiler/scope.h"
#include "compiler/eval.h"
#include "compiler/utils.h"
#include "compiler/ast-ops.h"

std::vector<std::string> eval_expr::const_storage;

eval_expr::eval_expr(std::unique_ptr<ast> expr_start, Ifunc_translation* fn, scope* cur_scope) : expr_node(std::move(expr_start)), 
fn_intf(fn), fn_scope(cur_scope) 
{
    const_storage.clear();
    const_storage.reserve(100);
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

bool eval_expr::hook() {
    expr = cast_to_ast_expr(expr_node);
    if(expr->is_fn_call()) {

       if(fn_scope->is_global_scope()) {
            sim_log_error("Function call expression not allowed in global scope");
       } 
       
       auto fn_expr = static_cast<ast_fn_call*>(const_cast<ast_expr*>(expr));

       if(fn_expr->fn_designator) {
            size_t num_args_found = fn_expr->children.size();
            
            auto fn_desig = std::move(fn_expr->fn_designator);
            std::string_view fn_name;

            //It's a function name
            if(cast_to_ast_expr(fn_desig)->is_var()) {
                fn_name = std::get<std::string>(cast_to_ast_token(fn_desig)->tok->value);
            }
            else {
                //It's an expression. Evaluate it like usual
                op_stack.push(std::move(expr_node));
                expr_node = std::move(fn_desig);
            }

            fn_call_stack.push(std::make_tuple(fn_name, num_args_found, 0, false));   
            return true;
       }
       else {
            auto& evaluated = std::get<3>(fn_call_stack.top());
            if(!evaluated) {
                //If no function name exists, it must have been an expression
                //Fetch the type of this expression
                if(!std::get<0>(fn_call_stack.top()).size()) {
                    auto& fn_desig = res_stack.top();
                    auto fn_type = fn_desig.type;
                    if(!fn_desig.type.is_function_type()) {
                        if(!(fn_desig.type.is_pointer_type() && fn_desig.type.resolve_type().is_function_type())) {
                            sim_log_error("Function designator must be a function type or pointer to function type");
                        }
                        fn_type = fn_desig.type.resolve_type(); 
                    }
                    size_t num_args = fn_type.mod_list[0].fn_spec.size();
                    std::get<2>(fn_call_stack.top()) = num_args;
                }
                evaluated = true;
            }
       } 
    }
    else if(expr->is_operator()) {
        auto op = cast_to_ast_op(expr_node);
        auto sym = std::get<operator_type>(op->tok->value);
        switch(sym) {
            case AND: 
            case OR: {
                if(expr->children.size() == 2) {
                    int false_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::create_label);
                    int end_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::create_label);

                    logical_stack.push(std::make_tuple(false_id, end_id));
                }
                else if(expr->children.size() == 1) {
                    auto res_expr_1 = fetch_stack_node(res_stack);

                    if(!res_expr_1.type.is_integral()) {
                        res_expr_1.convert_to_integral_type(fn_intf);
                    }

                    if(res_expr_1.is_constant) {
                        bool is_zero = std::stoi(std::string(res_expr_1.constant)) == 0; 
                        if((sym == AND && is_zero) || (sym == OR && !is_zero)) {
                            //Jump to false/true end if constant is 0/1(depending on context)
                            code_gen::call_code_gen(fn_intf, &Ifunc_translation::branch, std::get<0>(logical_stack.top()));
                        }
                    }
                    else {
                        //Branch if zero to the false end
                        void (Ifunc_translation::*branch_op)(int, int) = (sym == AND) ? &Ifunc_translation::branch_if_z : &Ifunc_translation::branch_if_nz;
                        code_gen::call_code_gen(fn_intf, branch_op, res_expr_1.expr_id, std::get<0>(logical_stack.top()));
                    }
                }
                break;
            }
        }
    }

    return false;
}

void eval_expr::handle_logical_op(operator_type op) {
    auto res_expr_2 = fetch_stack_node(res_stack);

    if(!res_expr_2.type.is_integral()) {
        res_expr_2.convert_to_integral_type(fn_intf);
    }

    //Todo: Make sure we get the common type of the 2 expressions
    //Right now we get the type of the 2nd expression
    expr_result res{res_expr_2.type};

    static const std::string_view pos_value = "1";
    static const std::string_view neg_value = "0";
    
    int false_id = 0, end_id = 0;
    if (op == AND || op == OR) {
        std::tie(false_id, end_id) = fetch_stack_node(logical_stack);
    }
    int tmp_id = 0; 
   
    if(res_expr_2.is_constant) {
        bool is_zero = std::stoi(std::string(res_expr_2.constant)) == 0; 
        if(op == AND || op == OR) {
            bool do_jmp = (op == AND && is_zero) || (op == OR && !is_zero);
            if(do_jmp) {
                code_gen::call_code_gen(fn_intf, &Ifunc_translation::branch, false_id);
            }
        }
        else if(op == NOT) {
            res.is_constant = true;
            res.constant = is_zero ? pos_value : neg_value;
        }
    }
    else if(op == AND || op == OR) {
        void (Ifunc_translation::*branch_op)(int, int) = (op == AND) ? &Ifunc_translation::branch_if_z : &Ifunc_translation::branch_if_nz;
        code_gen::call_code_gen(fn_intf, branch_op, res_expr_2.expr_id, false_id);
    } 
    switch(op) {
        case AND: 
        case OR: {
            std::string_view first_val = op == AND ? pos_value : neg_value;
            std::string_view second_val = op == AND ? neg_value : pos_value;
            tmp_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::create_temporary_value, res.type.base_type, res.type.is_signed);
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::set_value, tmp_id, first_val);
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::branch, end_id);
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::insert_label, false_id);
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::set_value, tmp_id, second_val);
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::insert_label, end_id);
            break;
        }
        case NOT: {
            if(!res_expr_2.is_constant) {
                tmp_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::if_eq, res_expr_2.expr_id, neg_value);
            }
            break;
        }
        default: {
            CRITICAL_ASSERT_NOW("handle_logical_op() called with invalid logical operator");
        }
    }

    res.expr_id = tmp_id;
    res_stack.push(res);
}

static bool check_if_symbol(std::string_view sym) {
    try {
        size_t idx = 0;
        int value = std::stoi(std::string(sym), &idx);
        return idx != sym.size();
    }
    catch(std::invalid_argument& _) {
        return true;
    }

    return true;
}

std::string_view eval_expr::arithmetic_with_symbol(std::string_view num1_str, std::string_view num2_str, operator_type binary_op) {
    auto sym1 = check_if_symbol(num1_str);
    auto sym2 = check_if_symbol(num2_str);
    if(!sym1 && !sym2)
        return "";
    
    if(sym1 && sym2) {
        sim_log_error("Arithmetic operation cannot be carried out on 2 symbols");
    }

    std::string_view sym = sym1 ? num1_str : num2_str;
    std::string_view number = sym1 ? num2_str : num1_str;

    if(binary_op != PLUS && binary_op != MINUS) {
        sim_log_error("Symbols can only be added or subtracted");
    }

    if(binary_op == PLUS) {
        const_storage.push_back(std::string(sym) + "+" + std::string(number));
    }
    else {
        const_storage.push_back(std::string(sym) + "-" + std::string(number));
    }

    return const_storage.back();
}

std::string_view eval_expr::constant_fold(std::string_view num1_str, operator_type unary_op) {
    if(check_if_symbol(num1_str)) {
        sim_log_error("Only addition and subtraction operations can be carried out in symbols");
    }

    int num1 = std::stoi(std::string(num1_str));

    switch(unary_op) {
        case PLUS: break;
        case MINUS: num1 = - num1; break;
        case BIT_NOT: num1 = ~num1; break;
        default: {
            CRITICAL_ASSERT_NOW("constant_fold_unary() called with invalid operator");
        }
    }

    const_storage.push_back(std::to_string(num1));
    return const_storage.back();
}

std::string_view eval_expr::constant_fold(std::string_view num1_str, std::string_view num2_str, operator_type binary_op) {
    
    std::string_view symbol_arithmetic = arithmetic_with_symbol(num1_str, num2_str, binary_op);
    if(symbol_arithmetic != "")
        return symbol_arithmetic;
    
    int num1 = std::stoi(std::string(num1_str));
    int num2 = std::stoi(std::string(num2_str));

    switch(binary_op) {
        case PLUS: num1 += num2; break;
        case MINUS: num1 -= num2; break;
        case MUL: num1 *= num2; break;
        case DIV: {
            if(num2 != 0)
                num1 /= num2;
            break;
        }
        case MODULO: {
            if(num2 != 0)
                num1 %= num2;
            break;
        } 
        case SHIFT_LEFT: num1 = num1 << num2 ; break;
        case SHIFT_RIGHT: num1 = num1 >> num2; break;
        case AMPER: num1 &= num2; break;
        case BIT_OR: num1 |= num2; break;
        case BIT_XOR: num1 ^= num2; break;
        case GT: num1 = num1 > num2 ? 1 : 0; break;
        case LT: num1 = num1 < num2 ? 1 : 0; break;
        case EQUAL_EQUAL: num1 = num1 == num2 ? 1 : 0; break;
        case NOT_EQUAL: num1 = num1 != num2 ? 1 : 0; break;
        default: {
            CRITICAL_ASSERT_NOW("constant_fold_bin() called with invalid operator");
        }
    }


    const_storage.push_back(std::to_string(num1));
    return const_storage.back();
}

bool eval_expr::handle_pointer_arithmetic(expr_result& res1, expr_result& res2, operator_type op) {
    
    if (!(res1.type.is_pointer_type() && res2.type.is_integral()) &&
        !(res1.type.is_integral() && res2.type.is_pointer_type())) {
        return false;
    }
    
    expr_result& res_p = res1.type.is_pointer_type() ? res1 : res2;
    expr_result& res_i = res2.type.is_integral() ? res2 : res1;
    

    if(op != PLUS && op != MINUS) {
        return false;
    }

    if(res_p.type.is_incomplete_type()) {
        sim_log_error("Pointer arithmetic cannot be carried out on a pointer to an incomplete type");
    }

    sim_log_debug("Performing pointer arithmetic");
    res_i.convert_type(res_p, fn_intf);

    int res_id = 0;
    std::string inc_count = std::to_string(res_p.type.get_pointer_base_type_size());
    std::string_view addend;
    if(res_i.is_constant) {
        addend = constant_fold(res_i.constant, inc_count, MUL);
    }
    else {
        res_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::mul, res_i.expr_id, std::string_view(inc_count));
    }
    
    int (Ifunc_translation::*op_var)(int, int); 
    int (Ifunc_translation::*op_var_con)(int, std::string_view);
    int (Ifunc_translation::*op_con_var)(std::string_view, int);
    if(op == PLUS) {
        op_var = &Ifunc_translation::add;
        op_var_con = &Ifunc_translation::add;
    }
    else {
        op_var = &Ifunc_translation::sub;
        op_var_con = &Ifunc_translation::sub;
        op_con_var = &Ifunc_translation::sub;
    }
    
    expr_result res{res_p.type};
    
    if(res_i.is_constant) {
        if(res_p.is_constant) {
            res.is_constant = true;
            res.constant = constant_fold(res_p.constant, addend, PLUS);
        }
        else 
            res_id = code_gen::call_code_gen(fn_intf, op_var_con, res_p.expr_id, addend);
    }
    else {
        res_id = code_gen::call_code_gen(fn_intf, op_var, res_p.expr_id, res_id);
    }

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

    if(res1.type.is_pointer_type() && res2.type.is_integral()) {
        handle_pointer_arithmetic(res1, res2, op);
        return;
    }

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
        case DIV: {
            is_commutative = false;
            op_vars = &Ifunc_translation::div; op_var_con = &Ifunc_translation::div;
            op_con_var = &Ifunc_translation::div;
            break;
        }
        case MODULO: {
            is_commutative = false;
            op_vars = &Ifunc_translation::modulo; op_var_con = &Ifunc_translation::modulo;
            op_con_var = &Ifunc_translation::modulo;
            break;
        }
        case AMPER: {
            op_vars = &Ifunc_translation::bit_and; op_var_con = &Ifunc_translation::bit_and;
            break;
        }
        case BIT_OR: {
            op_vars = &Ifunc_translation::bit_or; op_var_con = &Ifunc_translation::bit_or;
            break;
        }
        case BIT_XOR: {
            op_vars = &Ifunc_translation::bit_xor; op_var_con = &Ifunc_translation::bit_xor;
            break;            
        }
        case SHIFT_LEFT: {
            is_commutative = false;
            op_vars = &Ifunc_translation::sl; op_var_con = &Ifunc_translation::sl;
            op_con_var = &Ifunc_translation::sl;
            break;
        }
        case SHIFT_RIGHT: {
            is_commutative = false;
            op_vars = &Ifunc_translation::sr; op_var_con = &Ifunc_translation::sr;
            op_con_var = &Ifunc_translation::sr;
            break;
        }
        case GT: {
            is_commutative = true;
            op_vars = &Ifunc_translation::if_gt; op_var_con = &Ifunc_translation::if_gt; 
            if(res1.is_constant && !res2.is_constant) {
                std::swap(res1, res2);
                op_var_con = &Ifunc_translation::if_lt;
            }
            break;
        }
        case LT: {
            is_commutative = true;
            op_vars = &Ifunc_translation::if_lt; op_var_con = &Ifunc_translation::if_lt; 
            if(res1.is_constant && !res2.is_constant) {
                std::swap(res1, res2);
                op_var_con = &Ifunc_translation::if_gt;
            }

            break;
        }
        case EQUAL_EQUAL: {
            op_vars = &Ifunc_translation::if_eq; op_var_con = &Ifunc_translation::if_eq; 
            break;
        }
        case NOT_EQUAL: {
            op_vars = &Ifunc_translation::if_neq; op_var_con = &Ifunc_translation::if_neq; 
            break;
        } 
        default: {
            CRITICAL_ASSERT_NOW("handle_arithmetic_op called with invalid operator");
        }
    }

    expr_result res{ res1.type };
    if(!res1.is_constant && !res2.is_constant) {
        res_id = code_gen::call_code_gen(fn_intf, op_vars, res1.expr_id, res2.expr_id);
    }
    else if(res1.is_constant && res2.is_constant) {
        res.is_constant = true;
        res.constant = constant_fold(res1.constant, res2.constant, op);
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

    res.expr_id = res_id;
    res_stack.push(res);
}

void eval_expr::handle_assignment() {
    auto res2 = fetch_stack_node(res_stack);
    auto res1 = fetch_stack_node(res_stack);

    if(res1.type.is_void()) {
        sim_log_error("LHS of '=' cannot be void type");
    }

    if(res2.type.is_void()) {
        sim_log_error("Cannot assign expression of type 'void' to anything");
    }

    if(!res1.is_lvalue || !res1.type.is_modifiable() || res1.is_constant) {
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
    
    if(fn_scope->is_global_scope()) {
        if(in.category == l_val_cat::GLOBAL) {
            res.is_constant = true;
            res.constant = std::get<std::string>(in.var_token->value);
        }
    }

    res_stack.push(res);
}

void eval_expr::handle_array_subscript() {
    auto res1 = fetch_stack_node(res_stack);
    auto res2 = fetch_stack_node(res_stack);

    if(fn_scope->is_global_scope()) {
        if((res1.is_constant && check_if_symbol(res1.constant)) || (res2.is_constant && check_if_symbol(res2.constant))) {
            CRITICAL_ASSERT_NOW("Array subscript operation evaluation is currently not supported for compile time expression");
        }
    }

    if(res1.type.is_convertible_to_pointer_type()) {
        res1.type.convert_to_pointer_type();
    }

    if(res2.type.is_convertible_to_pointer_type()) {
        res2.type.convert_to_pointer_type();
    }
    
    if(!handle_pointer_arithmetic(res1, res2, PLUS)) {
        sim_log_error("[] operator requires operands to be pointer type and integral type");
    }
    handle_indir();
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

    auto res_type = res_in.type.resolve_type();

    if(res_type.is_void()) {
        sim_log_error("'*' operator can only be used on a non void pointer type");
    }

    if(fn_scope->is_global_scope() && res_in.is_constant && check_if_symbol(res_in.constant)) {
        sim_log_error("'*' operator cannot be used to evaluate symbol during compile time");
    }

    expr_result res{res_type};
    auto [base_type, is_signed] = res.type.get_simple_type();

    res.expr_id = res_in.expr_id;
    res.is_constant = res_in.is_constant;
    res.constant = res_in.constant;

    if(!(is_assignable() || res_type.is_array_type() || res_type.is_function_type())) {
        if(res_in.is_constant) {
            res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::fetch_from_mem, res_in.constant, base_type, is_signed); 
        }
        else {
            res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::fetch_from_mem, res_in.expr_id, base_type, is_signed); 
        }
    }

    res.category = l_val_cat::INDIR;
    res.is_lvalue = true;
    res_stack.push(res);
}

void eval_expr::handle_inc_dec(operator_type op, bool is_postfix) {
    auto in = fetch_stack_node(res_stack);
    if(!in.is_lvalue || !in.type.is_modifiable() || in.is_constant) {
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

void eval_expr::handle_simple_unary_op(operator_type op) {
    auto in = fetch_stack_node(res_stack);
    expr_result res{in.type};

    if(in.is_constant) {
        res.is_constant = true;
        res.constant = constant_fold(in.constant, op);
    }
    else {
        switch(op) {
            case PLUS: {
                res.expr_id = in.expr_id;
                break;
            }
            case MINUS: {
                res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::negate, in.expr_id);
                break;
            }
            case BIT_NOT: {
                res.expr_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::bit_not, in.expr_id);
            }
            default: {
                CRITICAL_ASSERT_NOW("handle_unary_op() failed as invalid operator was passed");
            }
        }
    }

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
        case NOT: {
            handle_logical_op(op);
            break;
        }
        case INCREMENT:  
        case DECREMENT: { 
            handle_inc_dec(op, false);
            break;
        }
        default: {
            handle_simple_unary_op(op);
        } 
    } 
}

void eval_expr::handle_var() {
    auto& var = fn_scope->fetch_var_info(std::get<std::string>(expr->tok->value));
    
    //Handles compile time computable expr case

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
    if(!is_assignable() && fn_scope->is_global_scope()) {
        if(var.type.is_array_type() || var.type.is_function_type()) {
            res.is_constant = true;
            res.constant = var.name;
        }
        else if(var.type.is_const()) {
            if(var.init_value != "") {
                res.is_constant = true;
                res.constant = var.init_value;
            }
        }
    }
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

void eval_expr::handle_fn_call() {
    auto [fn_name, num_args_found, num_args, _] = fetch_stack_node(fn_call_stack);

    auto check_if_function_type = [] (auto& fn_type) {
        if(!fn_type.is_function_type()) {
            if(!fn_type.is_pointer_to_function()) {
                sim_log_error("Function designator is not a function or pointer to function type");
            }
            fn_type = fn_type.resolve_type();
        }
    };

    //This implies it's a function name
    expr_result fn_desig{};
    type_spec fn_type{};
    bool is_fn_pointer = false;
    if(fn_name.size()) {
        auto& fn_info = fn_scope->fetch_var_info(fn_name);
        fn_type = fn_info.type;
        if(fn_type.is_pointer_to_function()) {
            fn_desig.expr_id = fn_info.var_id;
            is_fn_pointer = true;
        }
        check_if_function_type(fn_type);
        num_args = fn_type.mod_list[0].fn_spec.size();    
    }

    if(num_args != num_args_found) {
        sim_log_error("Number of arguments mentioned in function call is different from the fn declaration");
    }

    std::deque<expr_result> args;
    while(num_args--) {
        args.push_front(fetch_stack_node(res_stack));
    }

    if (!fn_name.size()) {
        fn_desig = fetch_stack_node(res_stack);
        fn_type = fn_desig.type;
        check_if_function_type(fn_type);
        is_fn_pointer = true;
    }
    
    size_t type_index = 0;
    for(auto& arg: args) {
        if(arg.type.is_array_type() || arg.type.is_function_type()) {
            arg.type.convert_to_pointer_type();   
        }
        arg.expr_id = type_spec::convert_type(fn_type.mod_list[0].fn_spec[type_index++], arg.expr_id, arg.type, fn_intf, !arg.is_constant);
    }
    //Once begin call frame is executed, only load_param call is allowed
    //This carries on until we execute call_function
    //Init the call frame and load the arguments
    code_gen::call_code_gen(fn_intf, &Ifunc_translation::begin_call_frame, num_args_found);    
    for(const auto& arg: args) {
        if(arg.is_constant) {
            auto [base_type, is_signed] = arg.type.get_simple_type();
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::load_param, arg.constant, base_type, is_signed);
        }
        else 
            code_gen::call_code_gen(fn_intf, &Ifunc_translation::load_param, arg.expr_id);
    }

    auto [ret_type, is_signed] = fn_type.resolve_type().get_simple_type();

    int ret_id;
    if(!is_fn_pointer)
        ret_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::call_function, fn_name, ret_type, is_signed);
    else 
        ret_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::call_function, fn_desig.expr_id, ret_type, is_signed);

    expr_result res{ret_id, fn_type.resolve_type()};
    res_stack.push(res);
}

void eval_expr::handle_comma_expr() {
    auto res_in = fetch_stack_node(res_stack);
    auto _ = fetch_stack_node(res_stack);

    _.free(fn_intf);
    res_in.is_lvalue = false;

    if(fn_scope->is_global_scope()) {
        res_in.is_constant = false;
    }
    res_stack.push(res_in);
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
                case AND:
                case OR: {
                    handle_logical_op(sym);
                    break;
                }
                case COMMA: {
                    handle_comma_expr();
                    break;
                }
                default: {
                    handle_arithmetic_op(sym);
                }
            }
        }
    }
    else if(expr->is_fn_call()) {
        handle_fn_call();
    }
    else if(expr->is_array_subscript()) {
        handle_array_subscript();
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
            if (hook())
                continue;
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