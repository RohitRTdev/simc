#pragma once
#include "compiler/ast.h"

static inline std::unique_ptr<ast> create_ast_program() {
    return std::make_unique<ast>(AST_TYPE::PROGRAM);
}

static inline std::unique_ptr<ast> create_ast_decl(const token* ident) {
    ast* node = static_cast<ast*>(new ast_decl(ident));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_decl_list(const token* type) {
    ast* node = static_cast<ast*>(new ast_decl_list(type));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_fn_decl(const token* ident, const token* type) {
    ast* node = static_cast<ast*>(new ast_fn_decl(ident, type));
    return std::unique_ptr<ast>(node);
}


static inline std::unique_ptr<ast> create_ast_fn_arg_list() {
    return std::make_unique<ast>(AST_TYPE::FN_ARG_LIST);
}


static inline std::unique_ptr<ast> create_ast_fn_arg(const token* type, const token* ident) {
    ast* node = static_cast<ast*>(new ast_fn_arg(type, ident));
    return std::unique_ptr<ast>(node);
}

static inline bool is_ast_decl_list(const std::unique_ptr<ast>& node) {
    return node->type == AST_TYPE::DECL_LIST;
}

static inline bool is_ast_decl(const std::unique_ptr<ast>& node) {
    return node->type == AST_TYPE::DECL;
} 

static inline bool is_ast_fn_arg(const std::unique_ptr<ast>& node) {
    return node->type == AST_TYPE::FN_ARG;
} 
