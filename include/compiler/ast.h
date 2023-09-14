#pragma once
#include <deque>
#include <memory>
#include "compiler/token.h"
#include "compiler/type.h"

enum class AST_TYPE {
    PROGRAM,
    BASE_TYPE,
    DECL_LIST,
    DECL,
    PTR_LIST,
    PTR_SPEC,
    ARRAY_SPEC_LIST,
    ARRAY_SPEC,
    PARAM_LIST,
    FN_DEF,
    STMT_LIST,
    EXPR_STMT,
    NULL_STMT,
    DECL_STMT,  
    RETURN,
    TOKEN,
    EXPR
};

struct ast {
private:
    AST_TYPE type;

#ifdef SIMDEBUG
    void print_ast_list(std::string_view msg);
#endif

public:
    std::deque<std::unique_ptr<ast>> children;
    ast(AST_TYPE _type); 

    void attach_node(std::unique_ptr<ast> node); 
    void attach_back(std::unique_ptr<ast> node); 
    std::unique_ptr<ast> remove_node(); 

    bool is_decl_list() const; 
    bool is_decl() const; 
    bool is_stmt() const;
    bool is_token() const;
    bool is_expr() const; 
    bool is_prog() const;
    bool is_fn_def() const;
    bool is_null_stmt() const;
    bool is_ret_stmt() const;
    bool is_decl_stmt() const;
    bool is_stmt_list() const;
    bool is_expr_stmt() const;
    bool is_pointer_list() const;
    bool is_array_specifier_list() const;
    bool is_base_type() const;
    bool is_param_list() const;

#ifdef SIMDEBUG
    virtual void print();
#endif 

    virtual ~ast() = default;
};

struct ast_base_type: ast {
    decl_spec base_type;

    ast_base_type(const token*, const token*, const token*, const token*, const token*);

#ifdef SIMDEBUG
    void print() override;
#endif
};

struct ast_ptr_spec : ast {
    const token* pointer, *const_qual, *vol_qual;
    ast_ptr_spec(const token* , const token* , const token* );

#ifdef SIMDEBUG
    void print() override;
#endif

};

struct ast_array_spec : ast {
    const token* constant;
    ast_array_spec(const token*);

#ifdef SIMDEBUG
    void print() override;
#endif

};

struct ast_decl : ast {
    const token* ident;
    ast_decl(const token*);

#ifdef SIMDEBUG
    void print() override;
#endif
};

struct ast_token: ast {
    const token* tok;
    ast_token(const token*);
    ast_token(const token*, bool);

#ifdef SIMDEBUG
    virtual void print() override;
#endif
};

enum class EXPR_TYPE {
    VAR,
    CON,
    OP,
    PUNC,
    FN_CALL
};

struct ast_expr : ast_token {
    EXPR_TYPE type;
    ast_expr(EXPR_TYPE _type);
    ast_expr(EXPR_TYPE _type, const token* tok);

    bool is_var() const;
    bool is_con() const;
    bool is_operator() const;
    bool is_lb() const;
    bool is_rb() const;
    bool is_comma() const;
    bool is_fn_call() const;

#ifdef SIMDEBUG
    virtual void print() override;
#endif
};

struct ast_op : ast_expr {
    size_t precedence;
    bool is_unary;
    bool is_postfix;
    bool is_lr;

    ast_op(const token* tok, bool unary, bool postfix, bool lr);


#ifdef SIMDEBUG
    void print() override;
#endif
private:
    void set_precedence(const token* tok);
};

struct ast_fn_call : ast_expr {
    std::unique_ptr<ast> fn_designator;
    ast_fn_call();

#ifdef SIMDEBUG
    void print() override;
#endif
};

std::unique_ptr<ast> create_ast_unary_op(const token* tok); 
std::unique_ptr<ast> create_ast_binary_op(const token* tok);
std::unique_ptr<ast> create_ast_postfix_op(const token* tok);
std::unique_ptr<ast> create_ast_expr_var(const token* tok); 
std::unique_ptr<ast> create_ast_expr_con(const token* tok); 
std::unique_ptr<ast> create_ast_punctuator(const token* tok); 
std::unique_ptr<ast> create_ast_fn_call(); 

bool is_ast_expr_lb(const std::unique_ptr<ast>& node); 
bool is_ast_expr_rb(const std::unique_ptr<ast>& node); 
bool is_ast_expr_comma(const std::unique_ptr<ast>& node); 
bool is_ast_expr_operator(const std::unique_ptr<ast>& node); 

const ast_op* cast_to_ast_op(const std::unique_ptr<ast>& node); 
const ast_expr* cast_to_ast_expr(const std::unique_ptr<ast>& node); 

