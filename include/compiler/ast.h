#pragma once
#include <deque>
#include <memory>
#include "compiler/token.h"

enum class AST_TYPE {
    PROGRAM,
    TOKEN,
    DECL_LIST,
    DECL,
    FN_DECL,
    FN_ARG_LIST,
    FN_ARG,
    FN_DEF,
    STMT_LIST,
    RETURN,
    NULL_STMT
};

struct ast {
private:
    AST_TYPE type;
public:
    std::deque<std::unique_ptr<ast>> children;
    ast(AST_TYPE _type); 

    void attach_node(std::unique_ptr<ast> node); 
    std::unique_ptr<ast> remove_node(); 

    bool is_decl_list(); 
    bool is_decl(); 
    bool is_fn_arg();
    bool is_stmt();
    bool is_token();

#ifdef SIMDEBUG
    virtual void print();
#endif 

    virtual ~ast() = default;
};

struct ast_token: ast {
    const token* tok;
    ast_token(const token*);

#ifdef SIMDEBUG
    void print() override;
#endif
};
