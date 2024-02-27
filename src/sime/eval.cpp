#include <string>
#include "core/token.h"
#include "core/ast.h"
#include "core/ast-ops.h"
#include "preprocessor/parser.h"


//Unary calculation
int calculate(operator_type op, int res) {
    int ans = 0;
    switch(op) {
        case PLUS: ans = res;
        case MINUS: ans = -res;
        case BIT_NOT: ans = ~res;
        case NOT: ans = !res;
        default: CRITICAL_ASSERT_NOW("calculate() unary called with invalid op:{}", op);
    }

    return ans;
}

//Binary calculation
int calculate(operator_type op, int res1, int res2) {
    int res = 0;
    switch(op) {
        case PLUS: res = res1 + res2; break;
        case MINUS: res = res1 - res2; break;
        case MUL: res = res1 * res2; break;
        case DIV: {
            if(res2 == 0) {
                print_error();
                sim_log_error("Division by zero encountered");
                res = 0;
            }
            else {
                res = res1 / res2;
            }
            break;
        }
        case MODULO: {
            if(res2 == 0) {
                print_error();
                sim_log_error("Division by zero encountered");
                res = 0;
            }
            else {
                res = res1 % res2;
            }
            break;
        }
        case AMPER: res = res1 & res; break;
        case BIT_OR: res = res1 | res2; break;
        case BIT_XOR: res = res1 ^ res2; break;
        case SHIFT_LEFT: res = res1 << res2; break;
        case SHIFT_RIGHT: res = res1 >> res2; break;
        case AND: res = res1 && res2; break;
        case OR: res = res1 || res2; break;
        case GT: res = res1 > res2; break;
        case LT: res = res1 < res2; break;
        case EQUAL_EQUAL: res = res1 == res2; break;
        default: CRITICAL_ASSERT_NOW("calculate() unary called with invalid op:{}", op);
    }


    return res;
}


int eval_expr(std::unique_ptr<ast> node) {
    int res = 0;
    auto expr = cast_to_ast_expr(node);
    if(expr->is_con()) {
        //Base case
        res = std::stoi(std::get<std::string>(cast_to_ast_token(node)->tok->value));
    }
    else if(expr->is_operator()) {
        auto op = cast_to_ast_op(node);
        operator_type op_type = std::get<operator_type>(cast_to_ast_token(node)->tok->value); 
        if(op->is_unary) {
            CRITICAL_ASSERT(node->children.size() == 1, "Unary operator found to have more than one operand");
            res = eval_expr(node->remove_node());
            res = calculate(op_type, res);
        }
        else {
            CRITICAL_ASSERT(node->children.size() == 2, "Binary operator found to not have exactly 2 operands");
            res = eval_expr(node->remove_node()); //Operand 2
            int res1 = eval_expr(node->remove_node()); //Operand 1
            res = calculate(op_type, res1, res);
        }
    }
    else {
        CRITICAL_ASSERT_NOW("eval_expr() called with invalid node type");
    }

    return res;
}



bool eval() {
    CRITICAL_ASSERT(g_ast->is_expr(), "eval() called with non expr ast node");

    int res = eval_expr(std::move(g_ast));
    sim_log_debug("Expression evaluated to:{}", res);
    return res != 0;
}
