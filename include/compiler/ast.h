#pragma once
#include <vector>
#include <memory>
#include "compiler/token.h"

enum class AST_TYPE {
    PROGRAM,
    DECL_LIST,
    FUNCTION_DEF
};

struct ast {
    AST_TYPE type;
    std::vector<std::unique_ptr<ast>> children;

    ast(AST_TYPE type);
    void attach_node(std::unique_ptr<ast>);
    std::unique_ptr<ast> remove_node();

    virtual ~ast() = default;
};

struct ast_decl : ast {
    token& decl_type;
    token& identifier;
};

std::unique_ptr<ast> create_ast_program();
