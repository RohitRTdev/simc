#include "compiler/ast.h"

ast::ast(AST_TYPE _type) : type(_type) 
{}

void ast::attach_node(std::unique_ptr<ast> node) {
    children.push_front(std::move(node));
}

std::unique_ptr<ast> ast::remove_node() {
    auto node = std::move(children.back());
    children.pop_back();
    return node;
}

ast_decl_list::ast_decl_list(const token* type) : ast(AST_TYPE::DECL_LIST), decl_type(type) 
{}

ast_decl::ast_decl(const token* ident) : ast(AST_TYPE::DECL), identifier(ident) 
{}

ast_fn_decl::ast_fn_decl(const token* ident, const token* type) : ast(AST_TYPE::FN_DECL), name(ident), ret_type(type)
{}

ast_fn_arg::ast_fn_arg(const token* type, const token* ident) : ast(AST_TYPE::FN_ARG), name(ident), decl_type(type)
{}

bool ast::is_ast_decl_list() {
    return type == AST_TYPE::DECL_LIST;
}

bool ast::is_ast_decl() {
    return type == AST_TYPE::DECL;
} 

bool ast::is_ast_fn_arg() {
    return type == AST_TYPE::FN_ARG;
} 
