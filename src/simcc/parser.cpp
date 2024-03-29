#include <functional>
#include <stack>
#include "debug-api.h"
#include "core/token.h"
#include "core/ast.h"
#include "core/ast-ops.h"
#include "core/state-machine.h"
#include "compiler/type.h"
#include "compiler/utils.h"

state_machine parser;

static void reduce_ret_stmt(state_machine* inst) {
    sim_log_debug("Reducing return statement");
    auto ret = create_ast_ret();
    if(inst->parser_stack.top()->is_expr())
        ret->attach_node(inst->fetch_parser_stack());
    
    auto ret_node = inst->fetch_parser_stack(); //Remove 'return' token
    ret->key_token = cast_to_ast_token(ret_node)->tok;
    inst->parser_stack.push(std::move(ret));
}

static void reduce_null_stmt(state_machine* inst) {
    sim_log_debug("Reducing null statement");
    inst->parser_stack.push(create_ast_null());
}

static void switch_after_reduce_stmt(state_machine* inst) {
//This means we go back to expecting another statement or switch to the state before the statement started
    if(inst->state_stack.top() == STMT_REDUCE) {
        auto stmt_list = create_ast_stmt_list();
        stmt_list->attach_node(inst->fetch_parser_stack());
        inst->parser_stack.push(std::move(stmt_list));
        inst->state_stack.pop();
    }

    switch(inst->state_stack.top()) {
        case STMT_LIST_REDUCE:
        case FN_DEF_REDUCE: inst->switch_state(EXPECT_STMT_LIST); break;
        case IF_STMT_REDUCE: inst->switch_state(EXPECT_ELSE_IF); break;
        case WHILE_STMT_REDUCE: {
            inst->switch_state(REDUCE_WHILE_STMT);
            break;
        }
        default: {
            inst->cur_token()->print_error();
            sim_log_error("Found statement in invalid context");
        } 
    }
}

static void reduce_while_stmt(state_machine* inst) {
    sim_log_debug("Reducing while stmt");

    CRITICAL_ASSERT(inst->state_stack.top() == WHILE_STMT_REDUCE, "reduce_while_stmt() called with wrong state");
    auto stmt_list = inst->fetch_parser_stack();
    inst->parser_stack.pop(); //Remove ')'
    auto expr = inst->fetch_parser_stack();
    
    auto ident = inst->fetch_parser_stack();
    CRITICAL_ASSERT(ident->is_token() && cast_to_ast_token(ident)->tok->is_keyword_while(), "Non while token found in stack during call to reduce_while_stmt()");
    CRITICAL_ASSERT(expr->is_expr() && stmt_list->is_stmt_list(), "Found erroneous tokens in stack during call to reduce_while_stmt()");

    auto node = create_ast_while();
    node->attach_node(std::move(stmt_list));
    node->attach_node(std::move(expr));

    inst->parser_stack.push(std::move(node));
    inst->state_stack.pop();

    switch_after_reduce_stmt(inst);
}

static void reduce_if_stmt(state_machine* inst) {
    sim_log_debug("Reducing if stmt");
    auto ast_node = inst->fetch_parser_stack();
    decltype(ast_node) expr, primary_node, stmt_list;
    auto if_node = create_ast_if();
    bool else_found = false;
    const token* else_if_token = nullptr;
    while(!(ast_node->is_token() && cast_to_ast_token(ast_node)->tok->is_keyword_if())) {
        if(ast_node->is_stmt_list())
            stmt_list = std::move(ast_node);
        else if(ast_node->is_expr()) 
            expr = std::move(ast_node);
        else if(!(ast_node->is_token() && cast_to_ast_token(ast_node)->tok->is_operator_rb()))
            primary_node = std::move(ast_node);

        if(primary_node) {
            if(cast_to_ast_token(primary_node)->tok->is_keyword_else_if()) {
                else_if_token = cast_to_ast_token(primary_node)->tok;
                auto node = create_ast_else_if();
                CRITICAL_ASSERT(expr, "Null expression detected for 'else if' node");
                node->attach_back(std::move(expr));
                node->attach_back(std::move(stmt_list));
                if_node->attach_node(std::move(node));
            }
            else if(cast_to_ast_token(primary_node)->tok->is_keyword_else()) {
                if(else_if_token) {
                    else_if_token->print_error();
                    sim_log_error("else if statement found after else statement");
                }
                if(else_found) {
                    cast_to_ast_token(primary_node)->tok->print_error();
                    sim_log_error("Found more than one else statement for if");
                }
                else_found = true;
                auto node = create_ast_else();
                CRITICAL_ASSERT(!expr, "Non Null expression detected for 'else' node");
                node->attach_back(std::move(stmt_list));
                if_node->attach_node(std::move(node));
            }
            primary_node.reset();
        }
        ast_node = inst->fetch_parser_stack();
    }
    if_node->attach_node(std::move(stmt_list));
    if_node->attach_node(std::move(expr));
    
    inst->parser_stack.push(std::move(if_node));
    inst->state_stack.pop();
    switch_after_reduce_stmt(inst);
}

static void reduce_break_or_continue_stmt(state_machine* inst) {
    if(inst->state_stack.top() == BREAK_STMT_REDUCE) {
        if(!cast_to_ast_token(inst->parser_stack.top())->tok->is_keyword_break()) {
            inst->cur_token()->print_error();
            sim_log_error("Invalid syntax for break statement");
        }
        auto break_node = inst->fetch_parser_stack();
        auto node = create_ast_break();
        node->key_token = cast_to_ast_token(break_node)->tok;
        inst->parser_stack.push(std::move(node));
    }
    else if(inst->state_stack.top() == CONTINUE_STMT_REDUCE) {
        if(!cast_to_ast_token(inst->parser_stack.top())->tok->is_keyword_continue()) {
            inst->cur_token()->print_error();
            sim_log_error("Invalid syntax for continue statement");
        }
        auto continue_node = inst->fetch_parser_stack();
        auto node = create_ast_continue();
        node->key_token = cast_to_ast_token(continue_node)->tok;
        inst->parser_stack.push(std::move(node));
    }
}

static void reduce_stmt(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case RETURN_STMT_REDUCE: reduce_ret_stmt(inst); inst->state_stack.pop(); break;
        case BREAK_STMT_REDUCE: 
        case CONTINUE_STMT_REDUCE: reduce_break_or_continue_stmt(inst); inst->state_stack.pop(); break;
        case STMT_LIST_REDUCE:
        case FN_DEF_REDUCE: 
        case STMT_REDUCE: {
            if(inst->parser_stack.top()->is_expr()) {
                auto expr = create_ast_expr_stmt();
                expr->attach_node(inst->fetch_parser_stack());
                inst->parser_stack.push(std::move(expr));
            }
            break;
        }
    }

    switch_after_reduce_stmt(inst);
}

static void reduce_null_stmt_sc(state_machine* inst) {
    reduce_null_stmt(inst);
    switch_after_reduce_stmt(inst);
}

static void reduce_stmt_list(state_machine* inst) {
    sim_log_debug("Reducing statement list");

    auto stmt_list = create_ast_stmt_list();
    while(!(inst->parser_stack.top()->is_token() && cast_to_ast_token(inst->parser_stack.top())->tok->is_operator_clb())) {
        stmt_list->attach_node(inst->fetch_parser_stack());
    }

    inst->parser_stack.pop(); //Remove '{'
    inst->parser_stack.push(std::move(stmt_list));

    auto pushed_state = fetch_stack_node(inst->state_stack);
    switch (pushed_state) {
        case STMT_LIST_REDUCE: switch_after_reduce_stmt(inst); break;
        case FN_DEF_REDUCE: inst->state_stack.push(FN_DEF_REDUCE); inst->switch_state(EXPECT_DECL_CRB); inst->stop_token_fetch(); break;
        default: {
            inst->cur_token()->print_error();
            sim_log_error("Invalid use of '}'");
        }
    }
}

static void reduce_expr(state_machine* inst, bool stop_at_lb = false, bool stop_at_comma = false, bool stop_at_lsb = false) {
    if(inst->parser_stack.empty() || !inst->parser_stack.top()->is_expr())
        return;

    sim_log_debug("Reducing expression");

    auto reduced_expr = inst->fetch_parser_stack();
    if(is_ast_pure_operator(reduced_expr)) {
        reduced_expr->print_error();
        sim_log_error("Invalid expression");
    }

    auto expr_eval = [&] {
        auto& top = inst->parser_stack.top();
        if(top->is_expr()) {
            return !(top->is_token() && cast_to_ast_token(top)->tok->is_operator_rb())
            && !is_ast_decl_equal(inst->parser_stack.top()) 
            && (stop_at_lb ? !is_ast_expr_lb(inst->parser_stack.top()) : true)
            && (stop_at_comma ? !is_ast_expr_comma(inst->parser_stack.top()) : true)
            && (stop_at_lsb ? !is_ast_expr_lsb(inst->parser_stack.top()) : true);
        }
        return false;
    };

    while(expr_eval()) {
        auto op_expr = inst->fetch_parser_stack();
        CRITICAL_ASSERT(is_ast_expr_operator(op_expr), "Non operator expression found after expression node");
        
        auto op = cast_to_ast_op(op_expr);
        CRITICAL_ASSERT(!op->children.size(), "Found non operator expression node after expression node");
        CRITICAL_ASSERT(!op->is_postfix, "Postfix operator found in unreduced state");
        
        if(op->is_unary) {
            op_expr->attach_node(std::move(reduced_expr));
        }
        else {
            auto left_expr = inst->fetch_parser_stack();
            CRITICAL_ASSERT(left_expr->is_expr(), "Found non expression node in parser stack during binary op reduction");
            op_expr->attach_node(std::move(reduced_expr));
            op_expr->attach_node(std::move(left_expr));
        }

        reduced_expr = std::move(op_expr);
    }

    inst->parser_stack.push(std::move(reduced_expr));
}

static void reduce_expr_comma(state_machine* inst) {
    sim_log_debug("Performing comma reduction");
    if(inst->state_stack.top() == FN_CALL_EXPR_REDUCE) {
        reduce_expr(inst, true, true);
    
        //Shift comma as punctuator
        inst->parser_stack.push(create_ast_punctuator(inst->cur_token()));
        inst->switch_state(EXPECT_EXPR_UOP_S); 
    }
    else {
        inst->cur_token()->print_error();
        sim_log_error("',' appeared in invalid context");
    }
}

static void reduce_decl_expr(state_machine* inst) {
    sim_log_debug("Performing init expr reduction");
    CRITICAL_ASSERT(inst->state_stack.top() == DECL_EXPR_REDUCE, "reduce_decl_expr() called in wrong context");
    reduce_expr(inst);

    if(is_ast_decl_equal(inst->parser_stack.top())) {
        inst->parser_stack.top()->print_error();
        sim_log_error("Expected an expression as initializer after '='");
    }

    inst->state_stack.pop();
    inst->stop_token_fetch();
    inst->switch_state(EXPECT_DECL_COMMA);
}

static void reduce_expr_bop(state_machine* inst) {
    if (inst->cur_token()->is_operator_comma()) {
        if (inst->state_stack.top() == FN_CALL_EXPR_REDUCE) {
            reduce_expr_comma(inst);
            return;
        }
        else if(inst->state_stack.top() == DECL_EXPR_REDUCE) {
           reduce_decl_expr(inst); 
           return;
        }
    }

    sim_log_debug("Performing binary op reduction");
    auto cur_op = create_ast_binary_op(inst->cur_token());
    auto cur_op_ptr = cast_to_ast_op(cur_op);
    CRITICAL_ASSERT(is_ast_expr_operator(cur_op), "Non operator expression found during binary op evaluation");
    auto reduced_expr = inst->fetch_parser_stack();
    
    while(inst->parser_stack.top()->is_expr() && 
    !is_ast_expr_comma(inst->parser_stack.top()) && 
    !is_ast_expr_lb(inst->parser_stack.top()) && 
    !is_ast_expr_lsb(inst->parser_stack.top()) &&
    !is_ast_decl_equal(inst->parser_stack.top())) {
        auto expr = inst->fetch_parser_stack();
        CRITICAL_ASSERT(is_ast_expr_operator(expr) && expr->children.size() == 0, "Unreduced expr operator found during binary op reduction"); 
         
        auto op = cast_to_ast_op(expr);
        if(op->is_unary) {
            expr->attach_node(std::move(reduced_expr));
        }
        else {
            if(op->precedence > cur_op_ptr->precedence) {
                auto left_expr = inst->fetch_parser_stack();
                expr->attach_node(std::move(reduced_expr));
                expr->attach_node(std::move(left_expr));
            }
            else if(op->precedence == cur_op_ptr->precedence) {
                if(op->is_lr) {
                    auto left_expr = inst->fetch_parser_stack();
                    expr->attach_node(std::move(reduced_expr));
                    expr->attach_node(std::move(left_expr));
                }
                else {
                    inst->parser_stack.push(std::move(expr));
                    inst->parser_stack.push(std::move(reduced_expr));
                    inst->parser_stack.push(std::move(cur_op));
                    break;
                }
            }
            else {
                //shift
                inst->parser_stack.push(std::move(expr));
                inst->parser_stack.push(std::move(reduced_expr));
                inst->parser_stack.push(std::move(cur_op));
                break;
            }
        }
        reduced_expr = std::move(expr);
    }
    if(reduced_expr)
        inst->parser_stack.push(std::move(reduced_expr));
    
    if(cur_op)
        inst->parser_stack.push(std::move(cur_op));
}

static void reduce_expr_postfix(state_machine* inst) {
    sim_log_debug("Reducing postfix expression");
    auto cur_op = create_ast_postfix_op(inst->cur_token());
    cur_op->attach_node(inst->fetch_parser_stack());
    inst->parser_stack.push(std::move(cur_op));
}

static void reduce_expr_rsb(state_machine* inst) {
    sim_log_debug("Reducing array subscript expression");
    if (inst->state_stack.top() != ARRAY_SUBSCRIPT_REDUCE) {
        inst->cur_token()->print_error();
        sim_log_error("Found ']' in wrong context");
    }

    reduce_expr(inst, true, false, true);

    if(is_ast_expr_lsb(inst->parser_stack.top())) {
        inst->parser_stack.top()->print_error();
        sim_log_error("array subscript [] cannot be empty");
    }

    auto expr_1 = inst->fetch_parser_stack();
    
    CRITICAL_ASSERT(is_ast_expr_lsb(inst->parser_stack.top()), "reduce_expr_rsb() called in wrong context!");
    auto lsb_key_node = inst->fetch_parser_stack(); //Remove '['
    auto expr_2 = inst->fetch_parser_stack(); 

    auto array_sub = create_ast_array_subscript(cast_to_ast_token(lsb_key_node)->tok);
    array_sub->attach_node(std::move(expr_1));
    array_sub->attach_node(std::move(expr_2));

    inst->parser_stack.push(std::move(array_sub));
    inst->state_stack.pop();
    inst->switch_state(EXPECT_EXPR_POP);
}

static void reduce_fn_call_expr(state_machine* inst) {
    sim_log_debug("Performing fn call reduction");
    reduce_expr(inst, true, true);

    auto fn_call = create_ast_fn_call();
    std::unique_ptr<ast> prev_expr;
    while(!is_ast_expr_lb(inst->parser_stack.top())) {
        auto expr = inst->fetch_parser_stack(); //Normal expression
        if(is_ast_expr_comma(expr))
            fn_call->attach_node(std::move(prev_expr));
        
        prev_expr = std::move(expr);
    }

    if(prev_expr)
        fn_call->attach_node(std::move(prev_expr));

    auto lb_key_node = inst->fetch_parser_stack(); //Remove '('
    //Attach '(' as key token for error diagnosis
    const_cast<ast_token*>(cast_to_ast_token(fn_call))->tok = cast_to_ast_token(lb_key_node)->tok;
    //Attach function designator
    static_cast<ast_fn_call*>(const_cast<ast_expr*>(cast_to_ast_expr(fn_call)))->fn_designator = inst->fetch_parser_stack();

    inst->parser_stack.push(std::move(fn_call));
}

static void reduce_expr_rb(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case LB_EXPR_REDUCE: {
            sim_log_debug("Performing lb reduction");
            reduce_expr(inst, true, false);
            if (is_ast_expr_lb(inst->parser_stack.top())) {
                inst->parser_stack.top()->print_error();
                sim_log_error("Found empty ()");
            }
            inst->switch_state(EXPECT_EXPR_POP);
            auto reduced_expr = inst->fetch_parser_stack();
            inst->parser_stack.pop(); //Remove '('
            inst->parser_stack.push(std::move(reduced_expr));
            inst->state_stack.pop();
            break;
        }
        case FN_CALL_EXPR_REDUCE: reduce_fn_call_expr(inst); inst->state_stack.pop(); inst->switch_state(EXPECT_EXPR_POP); break;
        case IF_STMT_REDUCE: 
        case WHILE_STMT_REDUCE: {
            sim_log_debug("Performing expr condition reduction"); 
            reduce_expr(inst, true, false);
            auto& top = inst->parser_stack.top();
            if (top->is_token() && cast_to_ast_token(top)->tok->is_operator_lb()) {
                if (inst->state_stack.top() == IF_STMT_REDUCE) {
                    inst->cur_token()->print_error();
                    sim_log_error("if statement must have condition");
                }
                else {
                    inst->cur_token()->print_error();
                    sim_log_error("while statement must have condition");
                }
            }
            auto expr = inst->fetch_parser_stack();
            inst->parser_stack.pop(); //Remove '('
            inst->switch_state(EXPECT_COMPOUND_STMT); 
            inst->parser_stack.push(std::move(expr));
            inst->parser_stack.push(create_ast_token(inst->cur_token())); //Push ')'
            break;
        }
        default: {
            inst->cur_token()->print_error();
            sim_log_error("Found ')' in invalid situation");
        }
    }
}

static void reduce_expr_empty_fn(state_machine* inst) {
    sim_log_debug("Reducing empty fn call expression");
    if(inst->state_stack.top() == FN_CALL_EXPR_REDUCE && is_ast_expr_lb(inst->parser_stack.top())) {
        inst->state_stack.pop();
        auto lb_key_node = inst->fetch_parser_stack(); //Remove '('
        auto fn_call = create_ast_fn_call();
        const_cast<ast_token*>(cast_to_ast_token(fn_call))->tok = cast_to_ast_token(lb_key_node)->tok;
        static_cast<ast_fn_call*>(const_cast<ast_expr*>(cast_to_ast_expr(fn_call)))->fn_designator = inst->fetch_parser_stack();
        inst->parser_stack.push(std::move(fn_call));
    }
    else {
        inst->cur_token()->print_error();
        sim_log_error("')' was expected as end of function call expression");
    } 
}

static void reduce_expr_stmt(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case STMT_LIST_REDUCE: 
        case FN_DEF_REDUCE:
        case STMT_REDUCE:
        case RETURN_STMT_REDUCE: {
            reduce_expr(inst, false, false);
            inst->switch_state(EXPECT_STMT_SC);
            inst->stop_token_fetch();
            break;
        }
        case DECL_EXPR_REDUCE: {
            reduce_decl_expr(inst);
            break;
        }
        default: { 
            inst->cur_token()->print_error();
            sim_log_error("';' found after expression in invalid context");
        }
    }
}

enum class base_reduction_context {
    EXTERNAL,
    INTERNAL,
    DECL_LB,
    PARAM_LIST
};

struct reduction_helpers {
    std::function<base_reduction_context (state_machine*)> base_reduce_context;
    std::function<bool (const std::unique_ptr<ast>&)> is_abstract;
    std::function<bool (const std::unique_ptr<ast>&)> is_empty;

    reduction_helpers() {
        base_reduce_context = [&] (state_machine* inst) {
            if(inst->state_stack.empty())
                return base_reduction_context::EXTERNAL;

            switch(inst->state_stack.top()) {
                case STMT_LIST_REDUCE:
                case STMT_REDUCE:
                case FN_DEF_REDUCE: return base_reduction_context::INTERNAL;
                case DECL_LB_REDUCE: return base_reduction_context::DECL_LB;
                case PARAM_LIST_REDUCE: return base_reduction_context::PARAM_LIST;
                default: return base_reduction_context::EXTERNAL;
            }            
        };

        is_abstract = [&] (const std::unique_ptr<ast>& declarator) {
            if(!static_cast<ast_decl*>(declarator.get())->ident)
                return true;
        
            return false;
        };

        is_empty = [&](const std::unique_ptr<ast>& declarator) {
            if (is_abstract(declarator) && !declarator->children.size())
                return true;

            return false;
        };
    }

}reduce_helper;

static void reduce_decl(state_machine* inst) {
    auto run_until = [&] {
        if(inst->parser_stack.top()->is_token()) {
            auto& tok = cast_to_ast_token(inst->parser_stack.top())->tok;
            
            if(tok->is_operator_lb() || tok->is_operator_comma()) {
                return false;
            }
        }
        else if(inst->parser_stack.top()->is_base_type()) {
            return false;
        }
        return true;
    };

    sim_log_debug("Reducing declarator");
    std::unique_ptr<ast> expr;
    //Declaration has an initializer
    if(inst->parser_stack.top()->is_expr()) {
        expr = inst->fetch_parser_stack();
        inst->parser_stack.pop(); //Remove '='
    }

    auto decl = create_ast_decl(nullptr);
    while(run_until()) {
        auto node = inst->fetch_parser_stack();

        if(node->is_array_specifier_list() || node->is_param_list()) {
            decl->attach_node(std::move(node));
        }
        else if(node->is_pointer_list()) {
            decl->attach_back(std::move(node));
        }
        else if(node->is_token() && cast_to_ast_token(node)->tok->is_identifier()) {
            static_cast<ast_decl*>(decl.get())->ident = cast_to_ast_token(node)->tok;
        }
        else if(node->is_decl()) {
            while(!decl->children.empty()) {
                auto spec = std::move(decl->children.front());
                decl->children.pop_front();
                node->attach_back(std::move(spec));
            }

            decl = std::move(node);
        }
        else {
            CRITICAL_ASSERT_NOW("Invalid ast node found while reducing decl");
        }
    }

    if(reduce_helper.is_abstract(decl)) {
        switch(reduce_helper.base_reduce_context(inst)) {
            case base_reduction_context::INTERNAL:
            case base_reduction_context::EXTERNAL: {
                inst->cur_token()->print_error();
                sim_log_error("Abstract declarator only allowed in parameter type lists");
            }
        }
    }

    if(expr) {
        switch(reduce_helper.base_reduce_context(inst)) {
            case base_reduction_context::INTERNAL:
            case base_reduction_context::EXTERNAL: break;
            default: {
                inst->cur_token()->print_error();
                sim_log_error("Initializer expression found in wrong context");
            }
        }
        decl->attach_back(std::move(expr));
    }

    inst->parser_stack.push(std::move(decl));
}
static void reduce_decl_comma(state_machine* inst) {

    if(reduce_helper.base_reduce_context(inst) == base_reduction_context::DECL_LB) {
        inst->cur_token()->print_error();
        sim_log_error("'(' appears to not be terminated");
    }

    reduce_decl(inst);
    
    if(reduce_helper.is_abstract(inst->parser_stack.top())) {
        switch(reduce_helper.base_reduce_context(inst)) {
            case base_reduction_context::INTERNAL:
            case base_reduction_context::EXTERNAL: {
                inst->cur_token()->print_error();
                sim_log_error("Abstract declarator only allowed in parameter type lists");
                break;
            }
        }
    }

    switch(reduce_helper.base_reduce_context(inst)) {
        case base_reduction_context::PARAM_LIST: {
            auto decl = inst->fetch_parser_stack();
            auto base_type = inst->fetch_parser_stack();
            CRITICAL_ASSERT(base_type->is_base_type(), "Declarator node was not followed by base type node");

            auto declaration = create_ast_decl_list();
            declaration->attach_node(std::move(decl));
            declaration->attach_node(std::move(base_type));
            inst->parser_stack.push(std::move(declaration));
            inst->switch_state(EXPECT_STOR_SPEC);
            break;
        }
        case base_reduction_context::INTERNAL:
        case base_reduction_context::EXTERNAL: inst->switch_state(EXPECT_DECL_POINTER); break;
    }

    if(!inst->cur_token()->is_operator_rb()) {
        inst->parser_stack.push(create_ast_token(inst->cur_token()));
    }
}
static void reduce_param_list(state_machine* inst) {
    
    CRITICAL_ASSERT(reduce_helper.base_reduce_context(inst) == base_reduction_context::PARAM_LIST, 
    "reduce_param_list() called in invalid context");

    auto run_until = [&] {
        return !(inst->parser_stack.top()->is_token() && 
        cast_to_ast_token(inst->parser_stack.top())->tok->is_operator_lb());
    };

    sim_log_debug("Reducing param list");
    auto param_list = create_ast_param_list();

    while(run_until()) {
        auto node = inst->fetch_parser_stack();

        if(node->is_decl_list()) {
            param_list->attach_node(std::move(node));
        }
        else if(!(node->is_token() && cast_to_ast_token(node)->tok->is_operator_comma())) {
            CRITICAL_ASSERT_NOW("Non ',' token found as delimiter while reducing param list");
        }
    }
    auto lb_key_node = inst->fetch_parser_stack(); //Remove '('
    param_list->key_token = cast_to_ast_token(lb_key_node)->tok;
    inst->parser_stack.push(std::move(param_list));
}

static void reduce_empty_param_list(state_machine* inst) {
    auto param_list = create_ast_param_list();
    inst->state_stack.pop();
    inst->parser_stack.pop(); //Remove '('

    inst->parser_stack.push(std::move(param_list));
}

static void reduce_decl_rb(state_machine* inst) {
    
    switch(reduce_helper.base_reduce_context(inst)) {
        case base_reduction_context::DECL_LB: {
            reduce_decl(inst);

            auto decl = inst->fetch_parser_stack();
            if (reduce_helper.is_empty(decl)) {
                if(!decl->print_error()) {
                    inst->cur_token()->print_error();
                }
                sim_log_error("Empty () usage introduces ambiguity. Is it declarator or empty function param list?");
            }
            inst->parser_stack.pop(); //Remove '('
            inst->parser_stack.push(std::move(decl));
            inst->state_stack.pop();
            break;
        }
        case base_reduction_context::PARAM_LIST: {
            reduce_decl_comma(inst);
            reduce_param_list(inst);
            inst->state_stack.pop();
            inst->switch_state(EXPECT_DECL_LSB);
            break;
        }
        default: {
            inst->cur_token()->print_error();
            sim_log_error("')' found in invalid context within a declarator");
        }
    }
}

static void reduce_array_specifier_list(state_machine* inst) {

    sim_log_debug("Reducing array specifier");
    auto arr_constant = inst->fetch_parser_stack();

    CRITICAL_ASSERT(arr_constant->is_token() && cast_to_ast_token(arr_constant)->tok->is_integer_constant(), 
    "Non integer constant node found while reducing array specifier");

    inst->parser_stack.pop(); //Remove '['
    auto arr_spec = create_ast_array_spec(cast_to_ast_token(arr_constant)->tok);

    if(inst->parser_stack.top()->is_array_specifier_list()) {
        inst->parser_stack.top()->attach_back(std::move(arr_spec));
    }
    else {
        auto list = create_ast_array_spec_list();
        list->attach_node(std::move(arr_spec));
        inst->parser_stack.push(std::move(list));
    }
}

static void reduce_pointer_list(state_machine* inst) {

    const token* const_qual = nullptr, *vol_qual = nullptr, *pointer = nullptr;

    sim_log_debug("Reducing pointer list");
    while(inst->parser_stack.top()->is_token()) {

        auto tok = cast_to_ast_token(inst->fetch_parser_stack())->tok;
        if(tok->is_keyword_const()) {
            if(const_qual) {
                tok->print_error();
                sim_log_error("const keyword appeared multiple times in pointer specifier");
            }
            const_qual = tok;
        }
        else if(tok->is_keyword_volatile()) {
            if(vol_qual) {
                tok->print_error();
                sim_log_error("volatile keyword appeared multiple times in pointer specifier");
            }
            vol_qual = tok;
        }
        else if(tok->is_operator_star()) {
            pointer = tok;
            break;
        }
        else {
            CRITICAL_ASSERT_NOW("Found unexpected token during pointer specifier reduction");
        }
    }

    auto ptr_spec = create_ast_ptr_spec(pointer, const_qual, vol_qual);
    if(inst->parser_stack.top()->is_pointer_list()) {
        inst->parser_stack.top()->attach_node(std::move(ptr_spec));
    }
    else {
        auto ptr_list = create_ast_ptr_list();
        ptr_list->attach_node(std::move(ptr_spec));
        inst->parser_stack.push(std::move(ptr_list));
    }

}

static void push_decl_lb_state(state_machine* inst) {
    inst->state_stack.push(DECL_LB_REDUCE);
}

static void push_stmt_reduce(state_machine* inst) {
    inst->state_stack.push(STMT_REDUCE);
}

static void reduce_decl_list(state_machine* inst) {

    if(reduce_helper.base_reduce_context(inst) == base_reduction_context::PARAM_LIST) {
        inst->cur_token()->print_error();
        sim_log_error("';' appeared within a parameter type list");
    }

    if(reduce_helper.base_reduce_context(inst) == base_reduction_context::DECL_LB) {
        inst->cur_token()->print_error();
        sim_log_error("'(' appears to not be terminated");
    }

    reduce_decl(inst);
    auto decl_list = create_ast_decl_list();

    while(!inst->parser_stack.top()->is_base_type()) {
        auto node = inst->fetch_parser_stack();

        if(node->is_decl())
            decl_list->attach_node(std::move(node));
        else if(!(node->is_token() && cast_to_ast_token(node)->tok->is_operator_comma())) {
            CRITICAL_ASSERT_NOW("Found invalid token while reducing decl list");
        }
    }

    auto base_type = inst->fetch_parser_stack();
    decl_list->attach_node(std::move(base_type));

    inst->parser_stack.push(std::move(decl_list));

    switch(reduce_helper.base_reduce_context(inst)) {
        case base_reduction_context::EXTERNAL: inst->switch_state(EXPECT_STOR_SPEC); break;
        case base_reduction_context::INTERNAL: {
            auto decl_list = inst->fetch_parser_stack();
            auto decl_stmt = create_ast_decl_stmt();
            decl_stmt->attach_node(std::move(decl_list));
            inst->parser_stack.push(std::move(decl_stmt));
 
            inst->switch_state(EXPECT_STMT_LIST); break;
        }
    }
}

static void reduce_base_type(state_machine* inst) {
    const token* storage_spec = nullptr;
    const token* sign_qual = nullptr;
    const token* int_modifier = nullptr;
    const token* core_type = nullptr;

    const token* const_qual = nullptr;
    const token* vol_qual = nullptr;
   
    auto extern_decl_check = [&] () {
        return !inst->parser_stack.empty() && !inst->parser_stack.top()->is_decl_list() && !inst->parser_stack.top()->is_fn_def();
    };

    auto intern_decl_check = [&] () {
        auto& top = inst->parser_stack.top();
        return !top->is_stmt() && !top->is_stmt_list() &&
        !(top->is_token() && cast_to_ast_token(top)->tok->is_operator_clb());
    };

    auto param_decl_check = [&] () {
        auto& top = inst->parser_stack.top();
        return !(top->is_token() && (cast_to_ast_token(top)->tok->is_operator_lb() ||
        cast_to_ast_token(top)->tok->is_operator_comma()));
    };

    auto run_until = [&] {
        switch(reduce_helper.base_reduce_context(inst)) {
            case base_reduction_context::EXTERNAL: return extern_decl_check();
            case base_reduction_context::INTERNAL: return intern_decl_check();
            case base_reduction_context::PARAM_LIST: return param_decl_check();
            default: CRITICAL_ASSERT_NOW("reduce_base_type() called in invalid context");
        }
    };

    sim_log_debug("Reducing base type");
    
    size_t num_tokens = 0;
    while(run_until()) {
        auto tok = cast_to_ast_token(inst->fetch_parser_stack())->tok;
        if(tok->is_storage_specifier()) {
            if(storage_spec) {
                tok->print_error();
                sim_log_error("More than one storage specifier mentioned in type");
            }

            storage_spec = tok;
        }
        else if(tok->is_data_type()) {
            if(tok->is_keyword_signed() || tok->is_keyword_unsigned()) {
                if(sign_qual) {
                    tok->print_error();
                    sim_log_error("signed/unsigned keyword mentioned more than once in type");
                }
                
                sign_qual = tok;
            }
            else {
                switch(std::get<keyword_type>(tok->value)) {
                    case TYPE_SHORT:
                    case TYPE_LONG:
                    case TYPE_LONGLONG: {
                        if(int_modifier) {
                            tok->print_error();
                            sim_log_error("short/long/longlong mentioned more than once");
                        } 
                        int_modifier = tok;
                        break;
                    }
                    default: {
                        if(core_type) {
                            tok->print_error();
                            sim_log_error("int/char mentioned more than once");
                        }
                        core_type = tok;
                        break;
                    }
                } 
            }
        }
        else if(tok->is_type_qualifier()) {
            if(tok->is_keyword_const()) {
                if(const_qual) {
                    tok->print_error();
                    sim_log_error("const mentioned more than once");
                }
                const_qual = tok;
            }
            else {
                if(vol_qual) {
                    tok->print_error();
                    sim_log_error("volatile mentioned more than once");
                }
                vol_qual = tok;
            }
        }
        else {
            CRITICAL_ASSERT_NOW("Found erroneous token while parsing base type");
        }
        num_tokens++;
    }

    if(!num_tokens) {
        inst->cur_token()->print_error();
        sim_log_error("Expected declaration to start with a declaration specifier");
    }

    if(reduce_helper.base_reduce_context(inst) == base_reduction_context::PARAM_LIST &&
    storage_spec && !storage_spec->is_keyword_register()) {
        storage_spec->print_error();
        sim_log_error("Only register storage specifier allowed in parameter type list");
    }

    if(!int_modifier && !core_type && !sign_qual) {
        inst->cur_token()->print_error();
        sim_log_error("type specifier must be mentioned in type");
    }

    if(int_modifier && core_type && (core_type->is_keyword_char() || core_type->is_keyword_void())) {
        core_type->print_error();
        sim_log_error("Invalid type specifier mentioned");
    }
    
    if (sign_qual && core_type && core_type->is_keyword_void()) {
        core_type->print_error();
        sim_log_error("void type cannot have signed/unsigned modifiers");
    }

    if(reduce_helper.base_reduce_context(inst) == base_reduction_context::EXTERNAL &&
    storage_spec && (storage_spec->is_keyword_auto() || storage_spec->is_keyword_register())) {
        storage_spec->print_error();
        sim_log_error("Only static/extern storage specifiers allowed in declaration at global scope");
    }

    auto type_spec = int_modifier ? int_modifier : core_type;

    inst->parser_stack.push(create_ast_base_type(storage_spec, type_spec, sign_qual, const_qual, vol_qual));
}

static void reduce_fn_decl(state_machine* inst) {
    sim_log_debug("Reducing fn declaration");
    reduce_decl_list(inst);

    switch(reduce_helper.base_reduce_context(inst)) {
        case base_reduction_context::DECL_LB:
        case base_reduction_context::PARAM_LIST: {
            inst->cur_token()->print_error();
            sim_log_error("'(' seems to be unterminated");
            break;
        }
        case base_reduction_context::INTERNAL: {
            inst->cur_token()->print_error();
            sim_log_error("Function cannot be defined within another function");
        }
    }

    auto& decl_list = inst->parser_stack.top();

    if(decl_list->children.size() != 2 || reduce_helper.is_abstract(decl_list->children[1]) || 
    decl_list->children[1]->children.size() < 1 || !decl_list->children[1]->children[0]->is_param_list()) {
        inst->cur_token()->print_error();
        sim_log_error("Invalid function declaration");
    }

    inst->parser_stack.push(create_ast_token(inst->cur_token()));
    inst->state_stack.push(FN_DEF_REDUCE);
    inst->switch_state(EXPECT_STMT_LIST);
}

static void reduce_fn_def(state_machine* inst) {
    auto fn = create_ast_fn_def();
    
    if(!inst->state_stack.size() || inst->state_stack.top() != FN_DEF_REDUCE) {
        inst->cur_token()->print_error();
        sim_log_error("}} found without accompanying {{");
    }

    fn->attach_node(inst->fetch_parser_stack()); //stmt list
    fn->attach_node(inst->fetch_parser_stack()); //fn decl

    inst->parser_stack.push(std::move(fn));
    inst->state_stack.pop();
}

static std::unique_ptr<ast> reduce_program(state_machine* inst) {
    //Attach all decl_list, fn def and fn decl to program node
    auto prog = create_ast_program();
    while(!inst->parser_stack.empty()) {
        prog->attach_node(inst->fetch_parser_stack());
    }

    return prog;
}

static void parse_compound_stmt() {
    parser.define_state("EXPECT_COMPOUND_STMT", EXPECT_STMT_LIST, &token::is_operator_clb, EXPECT_SINGLE_STMT, true, nullptr, nullptr, STMT_LIST_REDUCE);
    parser.define_reduce_state("EXPECT_SINGLE_STMT", EXPECT_STMT, push_stmt_reduce);
}

static void parse_if_stmt() {
    parser.define_state("EXPECT_STMT_IF", EXPECT_IF_LB, &token::is_keyword_if, EXPECT_STMT_WHILE, true, nullptr, nullptr, IF_STMT_REDUCE);
    parser.define_state("EXPECT_IF_LB", EXPECT_EXPR_UOP, &token::is_operator_lb, PARSER_ERROR, true, nullptr, 
    "Expected '(' after 'if' keyword");
    parser.define_state("EXPECT_ELSE_IF", EXPECT_IF_LB, &token::is_keyword_else_if, EXPECT_ELSE, true);
    parser.define_state("EXPECT_ELSE", EXPECT_COMPOUND_STMT, &token::is_keyword_else, REDUCE_IF_STMT, true);
    parser.define_reduce_state("REDUCE_IF_STMT", PARSER_ERROR, reduce_if_stmt);
}

static void parse_while_stmt() {
    parser.define_state("EXPECT_STMT_WHILE", EXPECT_WHILE_LB, &token::is_keyword_while, EXPECT_STMT_BREAK, true, nullptr, nullptr, WHILE_STMT_REDUCE);
    parser.define_state("EXPECT_WHILE_LB", EXPECT_EXPR_UOP, &token::is_operator_lb, PARSER_ERROR, true, nullptr,
    "Expected '(' after while keyword");
    parser.define_reduce_state("REDUCE_WHILE_STMT", PARSER_ERROR, reduce_while_stmt);
}

static void parse_stmt_list() {

    parser.define_state("EXPECT_STMT_LIST", EXPECT_STMT_LIST, &token::is_operator_clb, EXPECT_STMT_CRB, true, nullptr, nullptr, STMT_LIST_REDUCE);
    parser.define_state("EXPECT_STMT", EXPECT_EXPR_UOP, &token::is_keyword_return, EXPECT_STMT_IF, true, nullptr, nullptr, RETURN_STMT_REDUCE);
    parser.define_state("EXPECT_STMT_BREAK", EXPECT_STMT_SC, &token::is_keyword_break, EXPECT_STMT_CONTINUE, true, nullptr, nullptr, BREAK_STMT_REDUCE);
    parser.define_state("EXPECT_STMT_CONTINUE", EXPECT_STMT_SC, &token::is_keyword_continue, EXPECT_NULL_STMT_SC, true, nullptr, nullptr, CONTINUE_STMT_REDUCE);
    parser.define_special_state("EXPECT_STMT_SC", &token::is_operator_sc, reduce_stmt, PARSER_ERROR, 
    "Expected statement list to end with '}'");
    parser.define_special_state("EXPECT_NULL_STMT_SC", &token::is_operator_sc, reduce_null_stmt_sc, EXPECT_EXPR_UOP);
    parser.define_special_state("EXPECT_STMT_CRB", &token::is_operator_crb, reduce_stmt_list, BASE_TYPE_CHECK_STMT);

    parser.define_state("BASE_TYPE_CHECK_STMT", EXPECT_STOR_SPEC, &token::is_storage_specifier, BASE_TYPE_CHECK_STMT_1, true);
    parser.define_state("BASE_TYPE_CHECK_STMT_1", EXPECT_STOR_SPEC, &token::is_data_type, BASE_TYPE_CHECK_STMT_2, true);
    parser.define_state("BASE_TYPE_CHECK_STMT_2", EXPECT_STOR_SPEC, &token::is_type_qualifier, EXPECT_STMT, true);
}

static void parse_expr() {

    //At start of expression
    parser.define_shift_state("EXPECT_EXPR_UOP", EXPECT_EXPR_UOP, &token::is_unary_operator, EXPECT_EXPR_VAR, create_ast_unary_op);
    parser.define_shift_state("EXPECT_EXPR_VAR", EXPECT_EXPR_POP, &token::is_identifier, EXPECT_EXPR_CON, create_ast_expr_var);
    parser.define_shift_state("EXPECT_EXPR_CON", EXPECT_EXPR_POP, &token::is_constant, EXPECT_EXPR_LB, create_ast_expr_con);
    parser.define_shift_state("EXPECT_EXPR_LB", EXPECT_EXPR_UOP_S, &token::is_operator_lb, EXPECT_EXPR_RB, create_ast_punctuator, nullptr, LB_EXPR_REDUCE);
    
    //After an expression
    parser.define_state("EXPECT_EXPR_POP", EXPECT_EXPR_POP, &token::is_postfix_operator, EXPECT_EXPR_BOP, false, reduce_expr_postfix);
    parser.define_state("EXPECT_EXPR_BOP", EXPECT_EXPR_UOP_S, &token::is_binary_operator, EXPECT_EXPR_FN_LB, false, reduce_expr_bop);
    parser.define_shift_state("EXPECT_EXPR_FN_LB", EXPECT_EXPR_UOP_S, &token::is_operator_lb, EXPECT_EXPR_LSB, create_ast_punctuator, nullptr, FN_CALL_EXPR_REDUCE);
    parser.define_shift_state("EXPECT_EXPR_LSB", EXPECT_EXPR_UOP_S, &token::is_operator_lsb, EXPECT_EXPR_RB, create_ast_punctuator, nullptr, ARRAY_SUBSCRIPT_REDUCE);

    //Terminal components in an expression
    parser.define_special_state("EXPECT_EXPR_RB", &token::is_operator_rb, reduce_expr_rb, EXPECT_EXPR_RSB); 
    parser.define_special_state("EXPECT_EXPR_RSB", &token::is_operator_rsb, reduce_expr_rsb, EXPECT_EXPR_SC);
    parser.define_special_state("EXPECT_EXPR_SC", &token::is_operator_sc, reduce_expr_stmt, PARSER_ERROR,  
    "Expected expression to terminate with ')' or ',' or ';'");

    //States that start an expression but appear after an existing expression
    parser.define_shift_state("EXPECT_EXPR_UOP_S", EXPECT_EXPR_UOP_S, &token::is_unary_operator, EXPECT_EXPR_VAR_S, create_ast_unary_op);
    parser.define_shift_state("EXPECT_EXPR_VAR_S", EXPECT_EXPR_POP, &token::is_identifier, EXPECT_EXPR_CON_S, create_ast_expr_var);
    parser.define_shift_state("EXPECT_EXPR_CON_S", EXPECT_EXPR_POP, &token::is_constant, EXPECT_EXPR_LB_S, create_ast_expr_con);
    parser.define_shift_state("EXPECT_EXPR_LB_S", EXPECT_EXPR_UOP_S, &token::is_operator_lb, EXPECT_EXPR_FN_RB_S, create_ast_punctuator, nullptr, LB_EXPR_REDUCE);
    parser.define_state("EXPECT_EXPR_FN_RB_S", EXPECT_EXPR_POP, &token::is_operator_rb, PARSER_ERROR, false, reduce_expr_empty_fn, 
    "Expected unary op, var, constant, '(' for start of expression");
}


static void parse_declaration() {

    //Parse base type
    parser.define_state("EXPECT_STOR_SPEC", EXPECT_STOR_SPEC, &token::is_storage_specifier, EXPECT_DATA_TYPE, true);
    parser.define_state("EXPECT_DATA_TYPE", EXPECT_STOR_SPEC, &token::is_data_type, EXPECT_TYPE_QUAL, true);
    parser.define_state("EXPECT_TYPE_QUAL", EXPECT_STOR_SPEC, &token::is_type_qualifier, REDUCE_BASE_TYPE, true);
    parser.define_reduce_state("REDUCE_BASE_TYPE", EXPECT_DECL_POINTER, reduce_base_type);    

    //Base type checker
    parser.define_state("BASE_TYPE_CHECK", EXPECT_STOR_SPEC, &token::is_storage_specifier, BASE_TYPE_CHECK_1, true, nullptr, nullptr, PARAM_LIST_REDUCE);
    parser.define_state("BASE_TYPE_CHECK_1", EXPECT_STOR_SPEC, &token::is_data_type, BASE_TYPE_CHECK_2, true, nullptr, nullptr, PARAM_LIST_REDUCE); 
    parser.define_state("BASE_TYPE_CHECK_2", EXPECT_STOR_SPEC, &token::is_type_qualifier, BASE_TYPE_REDIRECTOR, true, nullptr, nullptr, PARAM_LIST_REDUCE);
    parser.define_reduce_state("BASE_TYPE_REDIRECTOR", EXPECT_DECL_POINTER, push_decl_lb_state);    

    //States before declarator
    parser.define_state("EXPECT_DECL_POINTER", EXPECT_POINTER_CONST, &token::is_operator_star, EXPECT_DECL_IDENT, true);
    parser.define_state("EXPECT_DECL_IDENT", EXPECT_DECL_LSB, &token::is_identifier, EXPECT_DECL_LB, true);
    parser.define_state("EXPECT_DECL_LB", BASE_TYPE_CHECK, &token::is_operator_lb, EXPECT_DECL_LSB, true);

    //States after declarator
    parser.define_state("EXPECT_DECL_LSB", EXPECT_ARRAY_CONSTANT, &token::is_operator_lsb, EXPECT_DECL_EQ, true);
    parser.define_state("EXPECT_DECL_EQ", EXPECT_EXPR_UOP, &token::is_operator_eq, EXPECT_FN_LB, true, nullptr, nullptr, DECL_EXPR_REDUCE);
    parser.define_state("EXPECT_FN_LB", EXPECT_FN_RB, &token::is_operator_lb, EXPECT_DECL_RB, true, nullptr, nullptr, PARAM_LIST_REDUCE);
    parser.define_state("EXPECT_FN_RB", EXPECT_DECL_LSB, &token::is_operator_rb, EXPECT_STOR_SPEC, false, reduce_empty_param_list);
    parser.define_state("EXPECT_DECL_RB", EXPECT_DECL_LSB, &token::is_operator_rb, EXPECT_DECL_COMMA, false, reduce_decl_rb);
    parser.define_special_state("EXPECT_DECL_COMMA", &token::is_operator_comma, reduce_decl_comma, EXPECT_DECL_SC);
    parser.define_special_state("EXPECT_DECL_SC", &token::is_operator_sc, reduce_decl_list, EXPECT_DECL_CLB);
    
    //Function definition
    parser.define_special_state("EXPECT_DECL_CLB", &token::is_operator_clb, reduce_fn_decl, PARSER_ERROR, 
    "Expected declaration to end with ';'");
    parser.define_state("EXPECT_DECL_CRB", EXPECT_STOR_SPEC, &token::is_operator_crb, PARSER_ERROR, false, reduce_fn_def, 
    "Expected function definition to end with }}");

    //Pointer states
    parser.define_state("EXPECT_POINTER_CONST", EXPECT_POINTER_VOLATILE, &token::is_keyword_const, EXPECT_POINTER_VOLATILE, true);
    parser.define_state("EXPECT_POINTER_VOLATILE", EXPECT_POINTER_CONST, &token::is_keyword_volatile, REDUCE_DECL_POINTER, true);
    parser.define_reduce_state("REDUCE_DECL_POINTER", EXPECT_DECL_POINTER, reduce_pointer_list);
    
    //Array specifier
    parser.define_state("EXPECT_ARRAY_CONSTANT", EXPECT_DECL_RSB, &token::is_integer_constant, PARSER_ERROR, true, nullptr,
    "Expected array specifier to have an integer constant");
    parser.define_state("EXPECT_DECL_RSB", EXPECT_DECL_LSB, &token::is_operator_rsb, PARSER_ERROR, false, reduce_array_specifier_list,
    "Expected array specifier to close with ']'");


}

void parse_init() {
    parse_declaration();
    parse_stmt_list();
    parse_compound_stmt();
    parse_if_stmt();
    parse_while_stmt();
    parse_expr();
}

std::unique_ptr<ast> parse() {
    
    static bool init_complete = false;

    if (!init_complete) {
        parse_init();
        init_complete = true;
    }
    
    parser.set_token_stream(tokens);
    parser.start();

    auto prog = reduce_program(&parser);
    CRITICAL_ASSERT(parser.parser_stack.empty(), "Parser stack not empty after parse");

#ifdef SIMDEBUG
    sim_log_debug("Printing AST");
    prog->print();
#endif
    return prog;
}
