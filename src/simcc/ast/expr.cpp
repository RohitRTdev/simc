#include "compiler/ast.h"
#include "compiler/ast-ops.h"
#include "debug-api.h"

ast_expr::ast_expr(EXPR_TYPE _type) : ast_token(nullptr, true), type(_type)
{}

ast_expr::ast_expr(EXPR_TYPE _type, const token* tok) : ast_token(tok, true), type(_type)
{}

ast_op::ast_op(const token* tok, bool unary, bool postfix, bool lr) : ast_expr(EXPR_TYPE::OP, tok), is_unary(unary), is_postfix(postfix), is_lr(lr)
{
    set_precedence(tok);   
}

ast_fn_call::ast_fn_call() : ast_expr(EXPR_TYPE::FN_CALL) 
{}

void ast_op::set_precedence(const token* tok) {
    if(is_unary && !is_postfix) {
        precedence = 14;
        return;
    }

    switch(std::get<operator_type>(tok->value)) {
        case INCREMENT:
        case DECREMENT: precedence = 15; break;
        case MUL:
        case DIV:
        case MODULO: precedence = 14; break;
        case PLUS:
        case MINUS: precedence = 13; break;
        case SHIFT_LEFT:
        case SHIFT_RIGHT: precedence = 12; break;
        case GT:
        case LT: precedence = 11; break;
        case EQUAL_EQUAL:
        case NOT_EQUAL: precedence = 10; break;
        case AMPER: precedence = 9; break;
        case BIT_XOR: precedence = 8; break;  
        case BIT_OR: precedence = 7; break;
        case AND: precedence = 6; break;    
        case OR: precedence = 5; break;
        case EQUAL: precedence = 3; break;  
        case COMMA: precedence = 1; break; 
        default: CRITICAL_ASSERT_NOW("Invalid operator given to set_precedence()");
    }
}

std::unique_ptr<ast> create_ast_unary_op(const token* tok) {
    ast* node = static_cast<ast*>(new ast_op(tok, true, false, false));

    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_binary_op(const token* tok) {
    ast* node = static_cast<ast*>(new ast_op(tok, false, false, !tok->is_operator_eq()));

    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_postfix_op(const token* tok) {
    ast* node = static_cast<ast*>(new ast_op(tok, true, true, true));

    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_expr_var(const token* tok) {
    ast* node = static_cast<ast*>(new ast_expr(EXPR_TYPE::VAR, tok));

    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_expr_con(const token* tok) {
    ast* node = static_cast<ast*>(new ast_expr(EXPR_TYPE::CON, tok));

    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_fn_call() {
    ast* node = static_cast<ast*>(new ast_fn_call());
    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_array_subscript() {
    ast* node = static_cast<ast*>(new ast_expr(EXPR_TYPE::ARRAY_SUB));
    return std::unique_ptr<ast>(node);
}

std::unique_ptr<ast> create_ast_punctuator(const token* tok) {
    ast* node = static_cast<ast*>(new ast_expr(EXPR_TYPE::PUNC, tok));

    return std::unique_ptr<ast>(node);
}

bool ast_expr::is_var() const {
    return type == EXPR_TYPE::VAR;
}

bool ast_expr::is_con() const {
    return type == EXPR_TYPE::CON;
}

bool ast_expr::is_operator() const {
    return type == EXPR_TYPE::OP;
}

bool ast_expr::is_lb() const {
    return type == EXPR_TYPE::PUNC && tok->is_operator_lb();
}

bool ast_expr::is_rb() const {
    return type == EXPR_TYPE::PUNC && tok->is_operator_rb();
}

bool ast_expr::is_comma() const {
    return type == EXPR_TYPE::PUNC && tok->is_operator_comma();
}

bool ast_expr::is_lsb() const {
    return type == EXPR_TYPE::PUNC && tok->is_operator_lsb();
}

bool ast_expr::is_rsb() const {
    return type == EXPR_TYPE::PUNC && tok->is_operator_rsb();
}

bool ast_expr::is_fn_call() const {
    return type == EXPR_TYPE::FN_CALL;
}

bool ast_expr::is_array_subscript() const {
    return type == EXPR_TYPE::ARRAY_SUB;
}


bool is_ast_expr_lb(const std::unique_ptr<ast>& node) {
    return node->is_expr() && cast_to_ast_expr(node)->is_lb();
}

bool is_ast_expr_rb(const std::unique_ptr<ast>& node) {
    return node->is_expr() && cast_to_ast_expr(node)->is_rb();
}

bool is_ast_expr_comma(const std::unique_ptr<ast>& node) {
    return node->is_expr() && cast_to_ast_expr(node)->is_comma();
}

bool is_ast_expr_lsb(const std::unique_ptr<ast>& node) {
    return node->is_expr() && cast_to_ast_expr(node)->is_lsb();
}

bool is_ast_expr_rsb(const std::unique_ptr<ast>& node) {
    return node->is_expr() && cast_to_ast_expr(node)->is_rsb();
}

bool is_ast_expr_operator(const std::unique_ptr<ast>& node) {
    return node->is_expr() && cast_to_ast_expr(node)->is_operator();
}

bool is_ast_pure_operator(const std::unique_ptr<ast>& node) {
    if(!is_ast_expr_operator(node))
        return false;
        
    auto op = cast_to_ast_op(node);
    if((op->is_unary || op->is_postfix) && op->children.size() < 1)
        return true;
    else if(op->children.size() < 2) 
        return true;

    return false;
}

bool is_ast_decl_equal(const std::unique_ptr<ast>& node) {
    return node->is_token() && cast_to_ast_token(node)->tok->is_operator_eq();
}

const ast_expr* cast_to_ast_expr(const std::unique_ptr<ast>& node) {
    auto _ptr = dynamic_cast<ast_expr*>(node.get());
    CRITICAL_ASSERT(_ptr, "cast_to_ast_expr() called with invalid ast node");
    return _ptr;
}

const ast_op* cast_to_ast_op(const std::unique_ptr<ast>& node) {
    auto _ptr = dynamic_cast<ast_op*>(node.get());
    CRITICAL_ASSERT(_ptr, "cast_to_ast_op() called with invalid ast node");
    return _ptr;
}
