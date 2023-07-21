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
private:
    AST_TYPE type;
public:
    std::deque<std::unique_ptr<ast>> children;
    ast(AST_TYPE _type); 

    void attach_node(std::unique_ptr<ast> node); 
    std::unique_ptr<ast> remove_node(); 

    bool is_ast_decl_list(); 
    bool is_ast_decl(); 
    bool is_ast_fn_arg();

#ifdef SIMDEBUG
    virtual void print();
#endif 

    virtual ~ast() = default;
};

struct ast_decl_list: ast {
    const token* decl_type;
    ast_decl_list(const token* type); 

#ifdef SIMDEBUG
    void print() override;
#endif
};

struct ast_decl : ast {
    const token* identifier;
    ast_decl(const token* ident); 

#ifdef SIMDEBUG
    void print() override;
#endif
};

struct ast_fn_decl : ast {
    const token* name;
    const token* ret_type;
    ast_fn_decl(const token* ident, const token* type);

#ifdef SIMDEBUG
    void print() override;
#endif
}; 

struct ast_fn_arg : ast {
    const token* decl_type;
    const token* name;
    ast_fn_arg(const token* type, const token* ident); 

#ifdef SIMDEBUG
    void print() override;
#endif
};
