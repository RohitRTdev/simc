#pragma once

#include "compiler/ast.h"

static inline std::unique_ptr<ast> create_ast_program() {
    return std::make_unique<ast>(AST_TYPE::PROGRAM);
}

static inline std::unique_ptr<ast> create_ast_decl(std::unique_ptr<ast> ident, std::unique_ptr<ast> value = std::unique_ptr<ast>()) {
    ast* node = static_cast<ast*>(new ast(AST_TYPE::DECL));
    if(value)
        node->attach_node(std::move(value));

    node->attach_node(std::move(ident));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_decl_list() {
    return std::make_unique<ast>(AST_TYPE::DECL_LIST);
}

static inline std::unique_ptr<ast> create_fn_decl() {
    return std::make_unique<ast>(AST_TYPE::FN_DECL);
}

static inline std::unique_ptr<ast> create_ast_fn_arg_list() {
    return std::make_unique<ast>(AST_TYPE::FN_ARG_LIST);
}

static inline std::unique_ptr<ast> create_ast_fn_arg(std::unique_ptr<ast> type, std::unique_ptr<ast> ident = std::unique_ptr<ast>()) {
    ast* node = static_cast<ast*>(new ast(AST_TYPE::FN_ARG));
    if(ident)
        node->attach_node(std::move(ident));
    node->attach_node(std::move(type));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_ret() {
    return std::make_unique<ast>(AST_TYPE::RETURN);
}

static inline std::unique_ptr<ast> create_ast_expr_stmt() {
    return std::make_unique<ast>(AST_TYPE::EXPR_STMT);
}

static inline std::unique_ptr<ast> create_ast_null() {
    return std::make_unique<ast>(AST_TYPE::NULL_STMT);
}

static inline std::unique_ptr<ast> create_ast_token(const token* tok) {
    ast* node = static_cast<ast*>(new ast_token(tok));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_stmt_list() {
    return std::make_unique<ast>(AST_TYPE::STMT_LIST);
}

static inline std::unique_ptr<ast> create_ast_fn_def() {
    return std::make_unique<ast>(AST_TYPE::FN_DEF);
}

static inline const ast_token* cast_to_ast_token(const std::unique_ptr<ast>& node) {
    return static_cast<ast_token*>(node.get());
}
