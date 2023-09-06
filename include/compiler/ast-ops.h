#pragma once

#include "compiler/ast.h"
#include "debug-api.h"

static inline std::unique_ptr<ast> create_ast_program() {
    return std::make_unique<ast>(AST_TYPE::PROGRAM);
}

static inline std::unique_ptr<ast> create_ast_decl(const token* ident) {
    ast* node = static_cast<ast*>(new ast_decl(ident));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_param_list() {
    return std::make_unique<ast>(AST_TYPE::PARAM_LIST);
}

static inline std::unique_ptr<ast> create_ast_decl_list() {
    return std::make_unique<ast>(AST_TYPE::DECL_LIST);
}

static inline std::unique_ptr<ast> create_ast_array_spec_list() {
    return std::make_unique<ast>(AST_TYPE::ARRAY_SPEC_LIST);
}

static inline std::unique_ptr<ast> create_ast_decl_stmt() {
    return std::make_unique<ast>(AST_TYPE::DECL_STMT);
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

static inline std::unique_ptr<ast> create_ast_ptr_list() {
    return std::make_unique<ast>(AST_TYPE::PTR_LIST);
}

static inline std::unique_ptr<ast> create_ast_token(const token* tok) {
    ast* node = static_cast<ast*>(new ast_token(tok));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_base_type(const token* storage_spec, const token* type_spec,
const token* sign_qual, const token* const_qual, const token* vol_qual) {
    ast* node = static_cast<ast*>(new ast_base_type(storage_spec, type_spec, const_qual, vol_qual, sign_qual));
    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_stmt_list() {
    return std::make_unique<ast>(AST_TYPE::STMT_LIST);
}

static inline std::unique_ptr<ast> create_ast_fn_def() {
    return std::make_unique<ast>(AST_TYPE::FN_DEF);
}

static inline std::unique_ptr<ast> create_ast_ptr_spec(const token* pointer, const token* const_qual,
const token* vol_qual) {
    auto node = static_cast<ast*>(new ast_ptr_spec(pointer, const_qual, vol_qual));

    return std::unique_ptr<ast>(node);
}

static inline std::unique_ptr<ast> create_ast_array_spec(const token* constant) {
    auto node = static_cast<ast*>(new ast_array_spec(constant));

    return std::unique_ptr<ast>(node);
}


static inline const ast_token* cast_to_ast_token(const std::unique_ptr<ast>& node) {
    auto _ptr = dynamic_cast<ast_token*>(node.get());
    CRITICAL_ASSERT(_ptr, "cast_to_ast_token called with invalid ast node");
    
    return _ptr;
}
