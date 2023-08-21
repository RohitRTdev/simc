#include <stack>
#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/ast.h"
#include "compiler/ast-ops.h"
#include "compiler/state-machine.h"

state_machine parser;

static void reduce_decl(state_machine* inst) {
    sim_log_debug("Reducing decl");
    auto tok = inst->fetch_parser_stack();

    //This means that decl is assigned a variable
    if(inst->parser_stack.top()->is_token() && cast_to_ast_token(inst->parser_stack.top())->tok->is_identifier()) {
        inst->parser_stack.push(create_ast_decl(inst->fetch_parser_stack(), std::move(tok)));
    }
    else {
        inst->parser_stack.push(create_ast_decl(std::move(tok)));
    }

}

static void reduce_decl_list(state_machine* inst) {
    sim_log_debug("Reducing decl list");
    auto decl_list = create_ast_decl_list();
    
    //Attach all decl's on top of stack to the created decl list
    while(!inst->parser_stack.top()->is_token()) {
        decl_list->attach_node(inst->fetch_parser_stack());
    }

    decl_list->attach_node(inst->fetch_parser_stack()); //Attach the type
    inst->parser_stack.push(std::move(decl_list));
}

static void reduce_fn_arg(state_machine* inst) {
    sim_log_debug("Reducing fn arg");
    std::unique_ptr<ast> ident;
    if(inst->parser_stack.top()->is_token() && cast_to_ast_token(inst->parser_stack.top())->tok->is_identifier()) {
        ident = inst->fetch_parser_stack();
    }

    inst->parser_stack.push(create_ast_fn_arg(inst->fetch_parser_stack(), std::move(ident))); 
}

static void reduce_fn_arg_list(state_machine* inst) {
    sim_log_debug("Reducing fn_arg_list");
    auto fn_arg_list = create_ast_fn_arg_list();
    while(inst->parser_stack.top()->is_fn_arg()) {
        fn_arg_list->attach_node(inst->fetch_parser_stack());
    }

    inst->parser_stack.push(std::move(fn_arg_list));
}

static void reduce_fn_decl(state_machine* inst) {
    sim_log_debug("Reducing function declaration");

    auto fn_decl = create_fn_decl();
    fn_decl->attach_node(inst->fetch_parser_stack()); //Fn arg list
    fn_decl->attach_node(inst->fetch_parser_stack()); //fn name
    fn_decl->attach_node(inst->fetch_parser_stack()); //fn ret type

    inst->parser_stack.push(std::move(fn_decl));
}

static void reduce_ret_stmt(state_machine* inst) {
    sim_log_debug("Reducing return statement");
    auto ret = create_ast_ret();
    if(inst->parser_stack.top()->is_expr())
        ret->attach_node(inst->fetch_parser_stack());
    inst->parser_stack.push(std::move(ret));
}

static void reduce_null_stmt(state_machine* inst) {
    sim_log_debug("Reducing null statement");
    inst->parser_stack.push(create_ast_null());
}

static void switch_after_reduce_stmt(state_machine* inst) {
//This means we go back to expecting another statement or switch to the state before the statement started
    switch(inst->state_stack.top()) {
        case EXPECT_STMT_LIST:
        case FN_DEF_REDUCE: inst->switch_state(EXPECT_STMT_LIST); break;
        default: sim_log_error("Found statement in invalid context");
    }
}

static void reduce_stmt(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case PARSER_STMT_RETURN: reduce_ret_stmt(inst); inst->state_stack.pop(); break;
        case EXPECT_STMT_LIST:
        case FN_DEF_REDUCE: {
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


    auto pushed_state = inst->state_stack.top();
    inst->state_stack.pop();

    switch (pushed_state) {
        case EXPECT_STMT_LIST: {
            auto new_state = inst->state_stack.top();

            switch (new_state) {
                case EXPECT_STMT_LIST:
                case FN_DEF_REDUCE: inst->switch_state(EXPECT_STMT_LIST); break;
                default: sim_log_error("Invalid use of '}'");
            }

            break;
        }
        case FN_DEF_REDUCE: inst->switch_state(FN_DEF_REDUCE); inst->stop_token_fetch(); break;
        default: sim_log_error("Invalid use of '}'");
    }
}

static void reduce_fn_def(state_machine* inst) {
    sim_log_debug("Reducing function definition");

    auto fn_def = create_ast_fn_def();
    fn_def->attach_node(inst->fetch_parser_stack()); //Get stmt list
    fn_def->attach_node(inst->fetch_parser_stack()); //Get function arg list
    fn_def->attach_node(inst->fetch_parser_stack()); //Get fn name
    fn_def->attach_node(inst->fetch_parser_stack()); //Get fn ret type

    inst->parser_stack.push(std::move(fn_def));
}

static void reduce_expr(state_machine* inst, bool stop_at_lb = false, bool stop_at_comma = false) {
    if(inst->parser_stack.empty() || !inst->parser_stack.top()->is_expr())
        return;

    sim_log_debug("Reducing expression");

    auto reduced_expr = inst->fetch_parser_stack();

    auto expr_eval = [&] {
        if(inst->parser_stack.top()->is_expr()) {
            return (stop_at_lb ? !is_ast_expr_lb(inst->parser_stack.top()) : true) && (stop_at_comma ? !is_ast_expr_comma(inst->parser_stack.top()) : true);
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

static void reduce_expr_bop(state_machine* inst) {
    sim_log_debug("Performing binary op reduction");
    auto cur_op = create_ast_binary_op(inst->cur_token());
    auto cur_op_ptr = cast_to_ast_op(cur_op);
    CRITICAL_ASSERT(is_ast_expr_operator(cur_op), "Non operator expression found during binary op evaluation");
    auto reduced_expr = inst->fetch_parser_stack();
    
    while(inst->parser_stack.top()->is_expr() && !is_ast_expr_comma(inst->parser_stack.top()) && !is_ast_expr_lb(inst->parser_stack.top())) {
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

static void reduce_expr_comma(state_machine* inst) {
    
    sim_log_debug("Performing comma reduction");
    if(inst->state_stack.top() == FN_CALL_EXPR_REDUCE) {
        reduce_expr(inst, true, true);
    
        //Shift comma
        inst->parser_stack.push(create_ast_punctuator(inst->cur_token()));
        inst->switch_state(EXPECT_EXPR_UOP_S);
    }
    else { 
        sim_log_error("',' found outside of a function call expression.");
    }
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

    inst->parser_stack.pop(); //Remove '('
    fn_call->attach_node(inst->fetch_parser_stack()); //Attach function designator

    inst->parser_stack.push(std::move(fn_call));
}

static void reduce_expr_rb(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case LB_EXPR_REDUCE: {
            sim_log_debug("Performing lb reduction");
            reduce_expr(inst, true, false);
            if (is_ast_expr_lb(inst->parser_stack.top()))
                sim_log_error("Found empty ()");

            inst->switch_state(EXPECT_EXPR_POP);
            auto reduced_expr = inst->fetch_parser_stack();
            inst->parser_stack.pop(); //Remove '('
            inst->parser_stack.push(std::move(reduced_expr));
            inst->state_stack.pop();
            break;
        }
        case FN_CALL_EXPR_REDUCE: reduce_fn_call_expr(inst); inst->state_stack.pop(); inst->switch_state(EXPECT_EXPR_POP); break;
        case EXPR_CONDITION_REDUCE: sim_log_debug("Performing expr condition reduction"); reduce_expr(inst, false, false); inst->state_stack.pop(); inst->switch_state(EXPECT_STMT_LIST); break;
        default: sim_log_error("Found ')' in invalid situation");
    }
}

static void reduce_expr_empty_fn(state_machine* inst) {
    if(inst->state_stack.top() == FN_CALL_EXPR_REDUCE && is_ast_expr_lb(inst->parser_stack.top())) {
        inst->state_stack.pop();
        inst->parser_stack.pop(); //Remove '('
        auto fn_call = create_ast_fn_call();
        fn_call->attach_node(inst->fetch_parser_stack());
        inst->parser_stack.push(std::move(fn_call));
    }
    else 
        sim_log_error("')' was expected as end of function call expression");
}

static void reduce_expr_stmt(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case EXPECT_STMT_LIST: 
        case FN_DEF_REDUCE:
        case PARSER_STMT_RETURN: reduce_expr(inst, false, false); inst->switch_state(EXPECT_STMT_SC); inst->stop_token_fetch(); break;
        default: sim_log_error("';' found after expression in invalid context");
    }
}

static std::unique_ptr<ast> reduce_program(state_machine* inst) {
    //Attach all decl_list, fn def and fn decl to program node
    auto prog = create_ast_program();
    while(!inst->parser_stack.empty()) {
        prog->attach_node(inst->fetch_parser_stack());
    }

    return prog;
}

static void parse_decl_list_init() {
    parser.define_state("EXPECT_DECL_COMMA_OR_SC", EXPECT_IDENT, &token::is_operator_comma, DECL_REDUCE, false, reduce_decl);
    parser.define_state("EXPECT_DECL_COMMA", EXPECT_DECL_IDENT, &token::is_operator_comma, EXPECT_DECL_EQ, false, reduce_decl);
    parser.define_reduce_state("DECL_REDUCE", EXPECT_DECL_SC, reduce_decl);
    parser.define_state("EXPECT_DECL_SC", PARSER_START, &token::is_operator_sc, PARSER_ERROR, false, reduce_decl_list, 
        "Expected ';' after declaration");
    parser.define_state("EXPECT_DECL_IDENT", EXPECT_DECL_EQ, &token::is_identifier, PARSER_ERROR, true, nullptr,
        "Expected identifier after data type in declaration");
    parser.define_state("EXPECT_DECL_EQ", EXPECT_DECL_VAR, &token::is_operator_eq, EXPECT_DECL_COMMA_OR_SC);
    parser.define_state("EXPECT_DECL_VAR", EXPECT_DECL_COMMA_OR_SC, &token::is_identifier, EXPECT_DECL_CON, true);
    parser.define_state("EXPECT_DECL_CON", EXPECT_DECL_COMMA_OR_SC, &token::is_constant, PARSER_ERROR, true, nullptr,
        "Expected identifier or constant after '='");
}

static void parse_stmt_list_init() {

    parser.define_state("EXPECT_STMT_LIST", EXPECT_STMT_LIST, &token::is_operator_clb, EXPECT_CRB, true, nullptr, nullptr, EXPECT_STMT_LIST);
    parser.define_state("EXPECT_STMT", EXPECT_EXPR_UOP, &token::is_keyword_return, EXPECT_NULL_STMT_SC, false, nullptr, nullptr, PARSER_STMT_RETURN);
    parser.define_special_state("EXPECT_STMT_SC", &token::is_operator_sc, reduce_stmt, PARSER_ERROR, 
    "Expected statement list to end with }}");
    parser.define_special_state("EXPECT_NULL_STMT_SC", &token::is_operator_sc, reduce_null_stmt_sc, EXPECT_EXPR_UOP);
    parser.define_special_state("EXPECT_CRB", &token::is_operator_crb, reduce_stmt_list, EXPECT_STMT);
    
}

static void parse_expr_init() {

    //At start of expression
    parser.define_shift_state("EXPECT_EXPR_UOP", EXPECT_EXPR_UOP, &token::is_unary_operator, EXPECT_EXPR_VAR, create_ast_unary_op);
    parser.define_shift_state("EXPECT_EXPR_VAR", EXPECT_EXPR_POP, &token::is_identifier, EXPECT_EXPR_CON, create_ast_expr_var);
    parser.define_shift_state("EXPECT_EXPR_CON", EXPECT_EXPR_POP, &token::is_constant, EXPECT_EXPR_LB, create_ast_expr_con);
    parser.define_shift_state("EXPECT_EXPR_LB", EXPECT_EXPR_UOP_S, &token::is_operator_lb, EXPECT_EXPR_RB, create_ast_punctuator, nullptr, LB_EXPR_REDUCE);
    
    //After an expression
    parser.define_state("EXPECT_EXPR_POP", EXPECT_EXPR_POP, &token::is_postfix_operator, EXPECT_EXPR_BOP, false, reduce_expr_postfix);
    parser.define_state("EXPECT_EXPR_BOP", EXPECT_EXPR_UOP_S, &token::is_binary_operator, EXPECT_EXPR_FN_LB, false, reduce_expr_bop); //reduce function for bop
    parser.define_shift_state("EXPECT_EXPR_FN_LB", EXPECT_EXPR_UOP_S, &token::is_operator_lb, EXPECT_EXPR_RB, create_ast_punctuator, nullptr, FN_CALL_EXPR_REDUCE);
    
    //Terminal components in an expression
    parser.define_special_state("EXPECT_EXPR_RB", &token::is_operator_rb, reduce_expr_rb, EXPECT_EXPR_COMMA); 
    parser.define_special_state("EXPECT_EXPR_COMMA", &token::is_operator_comma, reduce_expr_comma, EXPECT_EXPR_SC); 
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

void parse_init() {

    parser.set_token_stream(tokens);

    parser.define_state("PARSER_START", EXPECT_IDENT, &token::is_keyword_data_type, PARSER_ERROR, true, nullptr,
    "Expected translation unit to start with newline or data_type");
    parser.define_state("EXPECT_IDENT", EXPECT_LB, &token::is_identifier, PARSER_ERROR, true, nullptr,
        "Expected an identifier after a data type");
    parser.define_state("EXPECT_LB", EXPECT_FN_ARG_LIST_START, &token::is_operator_lb, EXPECT_DECL_COMMA);
    parser.define_state("EXPECT_FN_ARG_LIST_START", EXPECT_FN_ARG_LIST_IDENT, &token::is_keyword_data_type, EXPECT_RB, true);
    parser.define_state("EXPECT_FN_ARG_LIST_IDENT", EXPECT_FN_ARG_LIST_COMMA, &token::is_identifier, EXPECT_FN_ARG_LIST_COMMA, true);
    parser.define_state("EXPECT_FN_ARG_LIST_COMMA", EXPECT_FN_ARG_LIST, &token::is_operator_comma, FN_ARG_REDUCE, false, reduce_fn_arg);
    parser.define_state("EXPECT_FN_ARG_LIST", EXPECT_FN_ARG_LIST_IDENT, &token::is_keyword_data_type, PARSER_ERROR, true, nullptr,
        "Expected data type after ',' in function argument list");
    parser.define_reduce_state("FN_ARG_REDUCE", EXPECT_RB, reduce_fn_arg);
    parser.define_state("EXPECT_RB", EXPECT_SC, &token::is_operator_rb, PARSER_ERROR, false, reduce_fn_arg_list,
        "Expected function signature to end with ')'");
    parser.define_state("EXPECT_SC", PARSER_START, &token::is_operator_sc, EXPECT_FN_CLB, false, reduce_fn_decl);
    parser.define_state("EXPECT_FN_CLB", EXPECT_STMT_LIST, &token::is_operator_clb, PARSER_ERROR, true, nullptr, 
    "Expected function body to start with {{", FN_DEF_REDUCE); 
    parser.define_state("FN_DEF_REDUCE", PARSER_START, &token::is_operator_crb, PARSER_ERROR, false, reduce_fn_def, 
        "Expected function definition to end with }}");

    parse_decl_list_init();
    parse_stmt_list_init();
    parse_expr_init();
}



std::unique_ptr<ast> parse() {
    
    static bool init_complete = false;

    if (!init_complete) {
        parse_init();
        init_complete = true;
    }

    parser.start();

    auto prog = reduce_program(&parser);
    CRITICAL_ASSERT(parser.parser_stack.empty(), "Parser stack not empty after parse");

#ifdef SIMDEBUG
    sim_log_debug("Printing AST");
    prog->print();
#endif

    return prog;
}
