#include "compiler/ast.h"

ast::ast(AST_TYPE _type) : type(_type) 
{}

ast_token::ast_token(const token* _tok) : ast(AST_TYPE::TOKEN), tok(_tok)
{}

ast_token::ast_token(const token* _tok, bool is_expr) : ast(AST_TYPE::EXPR), tok(_tok)
{}

void ast::attach_node(std::unique_ptr<ast> node) {
    children.push_front(std::move(node));
}

std::unique_ptr<ast> ast::remove_node() {
    auto node = std::move(children.back());
    children.pop_back();
    return node;
}

bool ast::is_decl_list() {
    return type == AST_TYPE::DECL_LIST;
}

bool ast::is_decl() {
    return type == AST_TYPE::DECL;
} 

bool ast::is_fn_arg() {
    return type == AST_TYPE::FN_ARG;
} 

bool ast::is_stmt() {
    switch(type) {
        case AST_TYPE::RETURN:
        case AST_TYPE::NULL_STMT: return true;
    }

    return false;
}

bool ast::is_token() {
    return type == AST_TYPE::TOKEN;
}

bool ast::is_expr() {
    return type == AST_TYPE::EXPR;
}