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

bool ast::is_decl_list() const {
    return type == AST_TYPE::DECL_LIST;
}

bool ast::is_decl() const {
    return type == AST_TYPE::DECL;
} 

bool ast::is_fn_arg() const {
    return type == AST_TYPE::FN_ARG;
} 

bool ast::is_stmt() const {
    switch(type) {
        case AST_TYPE::RETURN:
        case AST_TYPE::NULL_STMT: return true;
    }

    return false;
}

bool ast::is_stmt_list() const {
    return type == AST_TYPE::STMT_LIST;
}

bool ast::is_token() const {
    return type == AST_TYPE::TOKEN;
}

bool ast::is_expr() const {
    return type == AST_TYPE::EXPR;
}

bool ast::is_prog() const {
    return type == AST_TYPE::PROGRAM;
}

bool ast::is_fn_decl() const {
    return type == AST_TYPE::FN_DECL;
}

bool ast::is_fn_def() const {
    return type == AST_TYPE::FN_DEF;
}

bool ast::is_null_stmt() const {
    return type == AST_TYPE::NULL_STMT;
}

bool ast::is_ret_stmt() const {
    return type == AST_TYPE::RETURN;
}

bool ast::is_expr_stmt() const {
    return type == AST_TYPE::EXPR_STMT;
}

