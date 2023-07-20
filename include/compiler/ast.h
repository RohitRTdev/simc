#pragma once
#include <deque>
#include <memory>
#include "compiler/token.h"

enum class AST_TYPE {
    PROGRAM,
    DECL_LIST,
    FUNCTION_DEF,
    DECL,
    FN_DECL,
    FN_ARG_LIST,
    FN_ARG
};

struct ast {
    AST_TYPE type;
    std::deque<std::unique_ptr<ast>> children;

    ast(AST_TYPE _type) : type(_type) 
    {}

    void attach_node(std::unique_ptr<ast> node) {
        children.push_front(std::move(node));
    }

    std::unique_ptr<ast> remove_node() {
        auto node = std::move(children.back());
        children.pop_back();
        return node;
    }

    virtual ~ast() = default;
};

struct ast_decl_list: ast {
    const token* decl_type;
    ast_decl_list(const token* type) : ast(AST_TYPE::DECL_LIST), decl_type(type) 
    {}
};

struct ast_decl : ast {
    const token* identifier;
    ast_decl(const token* ident) : ast(AST_TYPE::DECL), identifier(ident) 
    {}
};

struct ast_fn_decl : ast {
    const token* name;
    const token* ret_type;
    ast_fn_decl(const token* ident, const token* type) : ast(AST_TYPE::FN_DECL), name(ident), ret_type(type)
    {}
};

struct ast_fn_arg : ast {
    const token* decl_type;
    const token* name;
    ast_fn_arg(const token* type, const token* ident) : ast(AST_TYPE::FN_ARG), name(ident), decl_type(type)
    {}
};

void print_ast(const std::unique_ptr<ast>&);
