#include "compiler/ast.h"


ast::ast(AST_TYPE _type) : type(_type) 
{}

void ast::attach_node(std::unique_ptr<ast> node) {
    children.push_back(std::move(node));
}

std::unique_ptr<ast> ast::remove_node() {
    auto node = std::move(children.back());
    children.pop_back();
    
    return node;
}

std::unique_ptr<ast> create_ast_program() {
    return std::make_unique<ast>(AST_TYPE::PROGRAM);
}