#include <optional>
#include "core/token.h"
#include "core/ast.h"
#include "core/ast-ops.h"
#include "core/state-machine.h"
#include "preprocessor/parser.h"
#include "debug-api.h"

state_machine parser;
std::vector<token> tokens;
diag* g_diag_inst;
size_t g_dir_start_idx;
std::unique_ptr<ast> g_ast;

static void reduce_expr(state_machine* inst, bool stop_at_lb = false) {
    if(inst->parser_stack.empty() || !inst->parser_stack.top()->is_expr())
        return;

    sim_log_debug("Reducing expression");

    auto reduced_expr = inst->fetch_parser_stack();
    if(is_ast_pure_operator(reduced_expr)) {
        print_error();
        sim_log_error("Invalid expression");
    }

    auto expr_eval = [&] {
        if(!inst->parser_stack.size()) {
            return false;
        }
        auto& top = inst->parser_stack.top();
        if(top->is_expr()) {
            return !(top->is_token() && cast_to_ast_token(top)->tok->is_operator_rb())
            && (stop_at_lb ? !is_ast_expr_lb(inst->parser_stack.top()) : true);
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
    
    while(inst->parser_stack.size() && inst->parser_stack.top()->is_expr() && 
    !is_ast_expr_lb(inst->parser_stack.top())) {
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

static void reduce_expr_rb(state_machine* inst) {
    switch(inst->state_stack.top()) {
        case LB_EXPR_REDUCE: {
            sim_log_debug("Performing lb reduction");
            reduce_expr(inst, true);
            if (is_ast_expr_lb(inst->parser_stack.top())) {
                print_error();
                sim_log_error("Found empty ()");
            }
            inst->switch_state(EXPECT_EXPR_BOP);
            auto reduced_expr = inst->fetch_parser_stack();
            inst->parser_stack.pop(); //Remove '('
            inst->parser_stack.push(std::move(reduced_expr));
            inst->state_stack.pop();
            break;
        }
        default: {
            print_error();
            sim_log_error("Found ')' in invalid situation");
        }
    }
}

static void parse_expr() {
    //At start of expression
    parser.define_shift_state("EXPECT_EXPR_UOP", EXPECT_EXPR_UOP, &token::is_unary_operator, EXPECT_EXPR_CON, create_ast_unary_op);
    parser.define_shift_state("EXPECT_EXPR_CON", EXPECT_EXPR_BOP, &token::is_constant, EXPECT_EXPR_LB, create_ast_expr_con);
    parser.define_shift_state("EXPECT_EXPR_LB", EXPECT_EXPR_UOP, &token::is_operator_lb, PARSER_ERROR, create_ast_punctuator, 
            "Expected expression to start with unary operator", LB_EXPR_REDUCE);
    
    //After an expression
    parser.define_state("EXPECT_EXPR_BOP", EXPECT_EXPR_UOP, &token::is_binary_operator, EXPECT_EXPR_RB, false, reduce_expr_bop);

    //Terminal components in an expression
    parser.define_special_state("EXPECT_EXPR_RB", &token::is_operator_rb, reduce_expr_rb, PARSER_ERROR,  
    "Expected binary operator");
}

void parse_init() {
    parse_expr();
}

bool parse() {
    static bool init_complete = false;

    if (!init_complete) {
        parse_init();
        init_complete = true;
    }
    
    parser.set_token_stream(tokens);
    parser.set_diag_inst(g_diag_inst, g_dir_start_idx);
    parser.start();

    reduce_expr(&parser);
    g_ast = parser.fetch_parser_stack();
    CRITICAL_ASSERT(parser.parser_stack.size() == 0, "parser_stack should be empty after parse step");

#ifdef SIMDEBUG
    sim_log_debug("Printing AST");
    g_ast->print();
#endif
    return parser.parse_success;
}

enum lexer_states {
    LEXER_START,
    EXTENDED_OPERATOR_TOKEN,
    LOOKAHEAD_FOR_CONSTANT,
    EXPECTING_INTEGER_CONSTANT,
    EXPECTING_STRING_LITERAL,
    EXPECTING_STRING_CONSTANT,
    LOOKAHEAD_FOR_STRING_CONSTANT,
    LOOKAHEAD_FOR_WHITE_SPACE,
    LOOKAHEAD_FOR_TICK,
    EXPECTING_CHARACTER,
    EXPECTING_ESCAPED_CHARACTER,
    EXPECTING_TICK,
    LEXER_INVALID_TOKEN
};

static std::optional<char> fetch_escape_character(char ch) {
    
    std::optional<char> es_ch;
    switch (ch) {
        case 't': es_ch = '\t'; break;
        case 'r': es_ch = '\r'; break;
        case 'n': es_ch = '\n'; break;
        case 'b': es_ch = '\b'; break;
        case '0': es_ch = '\0'; break;
        case 'a': es_ch = '\a';
    }
    
    return es_ch;
}

static inline bool is_extended_operator(operator_type type) {
    switch (type) {
        case INCREMENT:
        case DECREMENT:
        case EQUAL_EQUAL:
        case NOT_EQUAL:
        case POINTER_TO:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
        case AND:
        case OR: return true;
    }

    return false;
}

bool lex(std::string_view input) {
    lexer_states state = LEXER_START;
    tokens.clear();

    operator_type prev_op;
    size_t start_pos = 0, literal_count = 0;
    for(int i = 0; i < input.size(); i++) {
        char ch = input[i];
        operator_type op;

        if (state == EXPECTING_TICK) {
            if (ch == '\'') {
                sim_log_debug("Found closing tick. Switching back to LEXER_START");
                state = LEXER_START;
                continue;
            }
            else {
                sim_log_debug("Stopping further evaluation as we found invalid character constant");
                return false;
            }
        }

        if (state == EXPECTING_ESCAPED_CHARACTER) {
            auto es_ch = fetch_escape_character(ch);
            if (es_ch) {
                sim_log_debug("Found escape character:{}", ch);
                tokens.push_back(token(CONSTANT, es_ch.value()));
                state = EXPECTING_TICK;
            }
            else {
                sim_log_debug("Stopping further evaluation as we found invalid escaped character constant");
                return false;
            }
        }

        if (state == EXPECTING_CHARACTER) {
            if (ch == '\\') {
                sim_log_debug("Found backslash. Switching state to EXPECTING_SINGLE_CHAR");
                state = EXPECTING_ESCAPED_CHARACTER;
            }
            else {
                tokens.push_back(token(CONSTANT, ch));
                state = EXPECTING_TICK;
            }
        }


        if(state == EXPECTING_STRING_CONSTANT) {
            if(ch == '\"') {
                std::string literal(input.begin()+start_pos+1, input.begin()+start_pos+literal_count+1);
                sim_log_debug("Pushing string constant:{} with literal_count: {}", literal, literal_count);
                if(!tokens.empty() && tokens.back().is_string_constant()) {
                    auto& str = std::get<std::string>(tokens.back().value);
                    str += literal; 
                }
                else {
                    tokens.push_back(token(CONSTANT, TOK_STRING, literal));
                }
                state = LEXER_START;
                continue;
            }
            else
                literal_count++;
        }

        if(state == EXPECTING_INTEGER_CONSTANT) {
            if(isdigit(ch)) {
                literal_count++;
            }
            else if(isalpha(ch) || ch == '_') {
                sim_log_debug("Stopping further evaluation as we found invalid integer constant");
                return false;
            }
            else {
                std::string num_literal(input.begin()+start_pos, input.begin()+start_pos+literal_count);
                tokens.push_back(token(CONSTANT, TOK_INT, num_literal));
                sim_log_debug("Pushed integer token:{}", num_literal);
                state = LEXER_START;
            }

        }

        if(state == EXPECTING_STRING_LITERAL) {
            //_ is also a valid character
            if(isalnum(ch) || ch == '_') {
                literal_count++;
            }
            else {
                std::string literal(input.begin()+start_pos, input.begin()+start_pos+literal_count);
                sim_log_debug("Identified literal:{} , literal_count:{}", literal, literal_count);

                sim_log_debug("Pushing identifier token:{}", literal);
                tokens.push_back(token(IDENT, literal));
                state = LEXER_START;
            }
        }

        if(state == EXTENDED_OPERATOR_TOKEN) {
            sim_log_debug("In state EXTENDED_OPERATOR_TOKEN.");
            op = prev_op;
            switch(ch) {
                case '+': {
                    if(prev_op == PLUS)
                        op = INCREMENT;
                    break;
                }
                case '-': {
                    if(prev_op == MINUS)
                        op = DECREMENT;
                    break;
                }
                case '&': {
                    if(prev_op == AMPER)
                        op = AND;
                    break;
                }
                case '|': {
                    if(prev_op == BIT_OR)
                        op = OR;
                    break;
                }
                case '>': {
                    if(prev_op == GT)
                        op = SHIFT_RIGHT;
                    else if(prev_op == MINUS)
                        op = POINTER_TO;
                    break;
                }
                case '<': {
                    if(prev_op == LT)
                        op = SHIFT_LEFT;
                    break;
                }
                case '=': {
                    if(prev_op == EQUAL)
                        op = EQUAL_EQUAL;
                    else if(prev_op == NOT)
                        op = NOT_EQUAL;
                    break;
                }
            }
            tokens.push_back(token(OPERATOR, op));
            sim_log_debug("Found operator_type:{}", op_debug[op]);
            state = LEXER_START;
            if (is_extended_operator(op))
                continue;
        }

        if(state == LEXER_START) {
            sim_log_debug("In state LEXER_START. Searching for operator token.");
            switch(ch) {
                case '~': op = BIT_NOT; break;
                case '*': op = MUL; break;
                case '/': op = DIV; break;
                case '%': op = MODULO; break;
                case '^': op = BIT_XOR; break;
                case '(': op = LB; break;
                case ')': op = RB; break;
                case '!': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = NOT;
                    break;
                } 
                case '+': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = PLUS;
                    break;
                }
                case '-': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = MINUS;
                    break;
                }
                case '&': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = AMPER;
                    break;
                }
                case '|': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = BIT_OR;
                    break;
                }
                case '<': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = LT;
                    break;
                }
                case '>': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = GT;
                    break;
                }
                case '=': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = EQUAL;
                    break;
                }
                default: {
                    state = LOOKAHEAD_FOR_CONSTANT;
                }
            }

            if(state == LEXER_START) {
                sim_log_debug("Pushing operator_type:{}", op_debug[op]);
                tokens.push_back(token(OPERATOR, op));
            }
        }

        if(state == LOOKAHEAD_FOR_CONSTANT) {
            if(isdigit(ch)) {
                sim_log_debug("Found digit.. Switching state to EXPECTING_INTEGER_CONSTANT");
                start_pos = i;
                literal_count = 1;
                state = EXPECTING_INTEGER_CONSTANT;
            }
            else if(isalpha(ch) || ch == '_') {
                sim_log_debug("Found ascii character.. Switching state to EXPECTING_STRING_LITERAL");
                start_pos = i;
                literal_count = 1;
                state = EXPECTING_STRING_LITERAL;
            } 
            else 
                state = LOOKAHEAD_FOR_TICK;
        }

        if(state == LOOKAHEAD_FOR_TICK) {
            if (ch == '\'') {
                sim_log_debug("Found single tick");
                state = EXPECTING_CHARACTER;
            }
            else
                state = LOOKAHEAD_FOR_STRING_CONSTANT;
        }

        if(state == LOOKAHEAD_FOR_STRING_CONSTANT) {
            if(ch == '\"') {
                sim_log_debug("Switching state to EXPECTING_STRING_CONSTANT");
                start_pos = i;
                literal_count = 0;
                state = EXPECTING_STRING_CONSTANT;
            }
            else
                state = LOOKAHEAD_FOR_WHITE_SPACE;
        }

        if (state == LOOKAHEAD_FOR_WHITE_SPACE) {
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
                state = LEXER_START;
            else
                state = LEXER_INVALID_TOKEN;
        }

        if(state == LEXER_INVALID_TOKEN) {
            print_error();
            sim_log_error("Invalid token encountered:'{}'", ch);
        }
    }

#ifdef SIMDEBUG
    print_token_list(tokens);
#endif
    CRITICAL_ASSERT(state == LEXER_START, "Lexical analysis failed");
    return true;
}

bool check_if_invalid() {
    for(auto& tok: tokens) {
        if (tok.pre_transform()) {
            return true;
        }
    }
    return false;
}

bool evaluator(std::string_view exp, diag* diag_inst, size_t dir_start_idx) {
    g_diag_inst = diag_inst;
    g_dir_start_idx = dir_start_idx;
    return lex(exp) && !check_if_invalid() && parse() && eval();
}

