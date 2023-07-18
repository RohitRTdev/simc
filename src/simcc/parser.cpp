#include <stack>
#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/token-ops.h"
#include "compiler/ast.h"

size_t token_idx = 0;

enum top_parser_states {
    PARSER_TOP,
    EXPECT_DECL_OR_FUNC
};

std::stack<std::unique_ptr<ast>> parser_stack;

void parse() {
    token_idx = 0;

    sim_log_debug("Start parser");
    top_parser_states state = PARSER_TOP; 


    auto prog = create_ast_program();

}

