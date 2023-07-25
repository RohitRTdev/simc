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
    if(inst->parser_stack.top()->is_token() && static_cast<ast_token*>(inst->parser_stack.top().get())->tok->is_identifier()) {
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
    if(inst->parser_stack.top()->is_token() && static_cast<ast_token*>(inst->parser_stack.top().get())->tok->is_identifier()) {
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
    inst->parser_stack.push(create_ast_ret());
}

static void reduce_null_stmt(state_machine* inst) {
    sim_log_debug("Reducing null statement");
    inst->parser_stack.push(create_ast_null());
}

static void reduce_stmt(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case PARSER_STMT_RETURN: reduce_ret_stmt(inst); break;
    }

    inst->state_stack.pop();

    //This means we go back to expecting another statement or switch to the state before the statement started
    if (inst->state_stack.top() == FN_DEF_REDUCE)
        inst->switch_state(EXPECT_STMT_LIST);
    else
        inst->switch_state(inst->state_stack.top());
}

static void reduce_null_stmt_sc(state_machine* inst) {
    reduce_null_stmt(inst);

    if (inst->state_stack.top() == FN_DEF_REDUCE)
        inst->switch_state(EXPECT_STMT_LIST);
    else
        inst->switch_state(inst->state_stack.top());
}

static void reduce_stmt_list(state_machine* inst) {
    sim_log_debug("Reducing statement list");

    auto stmt_list = create_ast_stmt_list();
    while(!(inst->parser_stack.top()->is_token() && static_cast<ast_token*>(inst->parser_stack.top().get())->tok->is_operator_clb())) {
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
        case FN_DEF_REDUCE: inst->switch_state(pushed_state); break;
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

static std::unique_ptr<ast> reduce_program(state_machine* inst) {
    //Attach all decl_list, fn def and fn decl to program node
    auto prog = create_ast_program();
    while(!inst->parser_stack.empty()) {
        prog->attach_node(inst->fetch_parser_stack());
    }

    return prog;
}

static void check_newline_valid(state_machine* inst) {
    switch (inst->state_stack.top()) {
        case EXPECT_STMT_LIST:
        case FN_DEF_REDUCE: inst->switch_state(EXPECT_STMT_LIST); break;
        default:
            sim_log_error("Invalid placement of newline");
    }
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

    parser.define_state("EXPECT_STMT_LIST", EXPECT_STMT_LIST, &token::is_operator_clb, EXPECT_STMT_NEWLINE, true, nullptr, nullptr, EXPECT_STMT_LIST);
    parser.define_special_state("EXPECT_STMT_NEWLINE", &token::is_newline, check_newline_valid, EXPECT_STMT);
    parser.define_state("EXPECT_STMT", EXPECT_STMT_SC, &token::is_keyword_return, EXPECT_NULL_STMT_SC, false, nullptr, nullptr, PARSER_STMT_RETURN);
    parser.define_special_state("EXPECT_STMT_SC", &token::is_operator_sc, reduce_stmt, EXPECT_CRB);
    parser.define_special_state("EXPECT_NULL_STMT_SC", &token::is_operator_sc, reduce_null_stmt_sc, EXPECT_CRB);
    parser.define_special_state("EXPECT_CRB", &token::is_operator_crb, reduce_stmt_list, PARSER_ERROR, "Expected '}' after statement list");
    
}


void parse_init() {

    parser.set_token_stream(tokens);

    parser.define_state("PARSER_START", EXPECT_IDENT, &token::is_keyword_data_type, EXPECT_NEWLINE, true);
    parser.define_state("EXPECT_NEWLINE", PARSER_START, &token::is_newline, PARSER_ERROR,
        false, nullptr, "Expected translation unit to start with newline or data_type");
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
    "Expected function body to start with '{'", FN_DEF_REDUCE); 
    parser.define_reduce_state("FN_DEF_REDUCE", PARSER_START, reduce_fn_def);

    parse_decl_list_init();
    parse_stmt_list_init();
}



void parse() {
    
    static bool init_complete = false;

    if (!init_complete) {
        parse_init();
        init_complete = true;
    }

    parser.start();
    auto prog = reduce_program(&parser);

#ifdef SIMDEBUG
    prog->print();
#endif 
}
