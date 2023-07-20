#include <stack>
#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/token-ops.h"
#include "compiler/ast.h"
#include "compiler/ast-ops.h"

size_t token_idx = 0;

enum parser_states {
    PARSER_START,
    PARSER_DECL_LIST,
    EXPECT_IDENT,
    EXPECT_DECL_COMMA_OR_SC,
    EXPECT_DECL_TYPE,
    EXPECT_FUNCTION_LB,
    EXPECT_FN_SC,
    EXPECT_FN_ARG_LIST,
    EXPECT_FN_ARG_IDENT,
    EXPECT_FN_COMMA,
    EXPECT_FN_RB,
    EXPECT_FN_ARG_LIST_START
};

std::stack<token*> token_stack;
std::stack<std::unique_ptr<ast>> parser_stack;
std::stack<parser_states> state_stack;

static inline token* fetch_token() {
    return &tokens[token_idx++];
}

static inline token* fetch_token_stack() {
    token* tok = token_stack.top();
    token_stack.pop();

    return tok;
}

static inline std::unique_ptr<ast> fetch_parser_stack() {
    auto node = std::move(parser_stack.top());
    parser_stack.pop();
    return node;
} 

static void reduce_decl() {
    sim_log_debug("Reducing decl");
    token* ident = fetch_token_stack();

    parser_stack.push(create_ast_decl(ident));
}

static void reduce_decl_list() {
    sim_log_debug("Reducing decl list");
    
    token* type = nullptr;
    if(is_token_data_type(*token_stack.top()))
        type = fetch_token_stack();

    auto decl_list = create_ast_decl_list(type);
    
    //Attach all decl's on top of stack to the created decl list
    while(!parser_stack.empty() && is_ast_decl(parser_stack.top())) {
        decl_list->attach_node(fetch_parser_stack());
    }

    parser_stack.push(std::move(decl_list));
}

static void reduce_fn_arg() {
    sim_log_debug("Reducing fn arg");
    token* ident = nullptr;
    if(is_token_identifier(*token_stack.top())) {
        ident = fetch_token_stack();
    }
    token* type = fetch_token_stack();

    parser_stack.push(create_ast_fn_arg(type, ident)); 
}

static void reduce_fn_arg_list() {
    sim_log_debug("Reducing fn_arg_list");
    auto fn_arg_list = create_ast_fn_arg_list();
    while(!parser_stack.empty() && is_ast_fn_arg(parser_stack.top())) {
        fn_arg_list->attach_node(fetch_parser_stack());
    }

    parser_stack.push(std::move(fn_arg_list));
}

static void reduce_fn_decl() {
    const token* ident = fetch_token_stack();
    const token* type = fetch_token_stack();

    auto fn_decl = create_fn_decl(ident, type);
    fn_decl->attach_node(fetch_parser_stack());

    parser_stack.push(std::move(fn_decl));
}


static std::unique_ptr<ast> reduce_program() {
    //Attach all decl_list, fn def and fn decl to program node
    auto prog = create_ast_program();
    while(!parser_stack.empty()) {
        prog->attach_node(fetch_parser_stack());
    }

    return prog;
}


void parse() {
    token_idx = 0;

    sim_log_debug("Start parser");
    parser_states state = PARSER_START;
    bool advance_token = true; 
    token* tok = nullptr;
    while(token_idx < tokens.size()) {
        if(advance_token)
            tok = fetch_token();


        if(state == EXPECT_FN_SC) {
            if(is_token_operator_sc(*tok)) {
                sim_log_debug("Found ';'. Reducing fn_decl");
                reduce_fn_decl();
                state = PARSER_START;
                continue;
            }
            else {
                sim_log_error("Expected ';' to terminate function declaration");
            }
        }


        if(state == EXPECT_FN_ARG_LIST) {
            if(is_token_data_type(*tok)) {
                sim_log_debug("Found data type. Expecting ident or ',' or ')'");
                token_stack.push(tok);
                state = EXPECT_FN_ARG_IDENT;
                continue;
            }
            else {
                sim_log_error("Expected data type in fn argument list");
            }
        }

        if(state == EXPECT_FN_COMMA) {
            if(is_token_operator_comma(*tok)) {
                sim_log_debug("Found ','. Expecting fn_arg_list");
                state = EXPECT_FN_ARG_LIST;
            }
            else {
                state = EXPECT_FN_RB;
            }
            reduce_fn_arg();
            advance_token = true;
        }

        if(state == EXPECT_FN_ARG_IDENT) {
            if(is_token_identifier(*tok)) {
                sim_log_debug("Found identifier. Expecting ',' or ')'");
                token_stack.push(tok);
            }
            else {
                advance_token = false;
            }
            state = EXPECT_FN_COMMA;
        }

        if(state == EXPECT_FN_ARG_LIST_START) {
            if(is_token_data_type(*tok)) {
                sim_log_debug("Found data type. Expecting ident or ',' or ')'");
                state = EXPECT_FN_ARG_IDENT;
                token_stack.push(tok);
            }
            else if(is_token_operator_rb(*tok)) {
                //Empty function arg list
                sim_log_debug("Found ')'(Empty function arg list). Expecting ';'");
                parser_stack.push(create_ast_fn_arg_list());
                state = EXPECT_FN_SC;
            } 
        }

        if(state == EXPECT_FN_RB) {
            if(is_token_operator_rb(*tok)) {
                sim_log_debug("Found ')'. Reducing fn_arg_list");
                state = EXPECT_FN_SC;
                reduce_fn_arg_list();
            }
            else {
                sim_log_error("Expected ')' to close fn declaration");
            }
        }

        if(state == EXPECT_FUNCTION_LB) {
            if(is_token_operator_lb(*tok)) {
                sim_log_debug("Found '('. Expecting fn_arg_list next or ')'");
                state = EXPECT_FN_ARG_LIST_START;
                state_stack.pop();
            }
            else {
                sim_log_debug("Found start of potential decl list");
                state_stack.top() = PARSER_DECL_LIST;
                state = EXPECT_DECL_COMMA_OR_SC;
            }
        }

        if(state == EXPECT_DECL_COMMA_OR_SC) {
            if(is_token_operator_comma(*tok)) {
                reduce_decl();
                state = EXPECT_IDENT;
            }
            else if(is_token_operator_sc(*tok)) {
                reduce_decl();
                reduce_decl_list();
                state = PARSER_START;
                state_stack.pop();
            }
            else {
                sim_log_error("Expected ',' or ';' for decl_list");
            }
            continue;
        }


        if(state == EXPECT_IDENT) {
            if(is_token_identifier(*tok)) {
                if(state_stack.top() == PARSER_START)
                    state = EXPECT_FUNCTION_LB; 
                else if(state_stack.top() == PARSER_DECL_LIST)
                    state = EXPECT_DECL_COMMA_OR_SC;
                sim_log_debug("Pushing identifier token:{}", std::get<std::string>(tok->value));
                token_stack.push(tok);
            }
            else {
                sim_log_error("Expected identifier token.");
            }
        }

        if(state == PARSER_START) {
            if(is_token_data_type(*tok)) {
                state = EXPECT_IDENT;
                sim_log_debug("Pushing keyword token:{}", keywords_debug[std::get<keyword_type>(tok->value)]);
                token_stack.push(tok);
                state_stack.push(PARSER_START);
            }
            else if(is_token_newline(*tok)) {
                state = PARSER_START;
            }
            else {
                sim_log_error("Translation unit must always start with a data_type keyword");
            }
        }
    }

    auto prog = reduce_program();

    sim_log_debug("Printing ast");
#ifdef SIMDEBUG
    print_ast(prog);
#endif

}

