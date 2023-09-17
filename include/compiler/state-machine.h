#pragma once

#include <vector>
#include <memory>
#include <stack>
#include "compiler/token.h"
#include "compiler/ast.h"


enum parser_states {
    //Base type
    EXPECT_STOR_SPEC,
    EXPECT_DATA_TYPE,
    EXPECT_TYPE_QUAL,
    REDUCE_BASE_TYPE,
    BASE_TYPE_CHECK,
    BASE_TYPE_CHECK_1,
    BASE_TYPE_CHECK_2,
    BASE_TYPE_REDIRECTOR,

    //Declarator
    EXPECT_DECL_POINTER,
    EXPECT_DECL_IDENT,
    EXPECT_DECL_LB,
    EXPECT_DECL_LSB,
    EXPECT_FN_LB,
    EXPECT_FN_RB,
    EXPECT_DECL_RB,
    EXPECT_DECL_COMMA,
    EXPECT_DECL_SC,
    EXPECT_DECL_CLB,
    EXPECT_DECL_CRB,

    //Pointer list
    EXPECT_POINTER_CONST,
    EXPECT_POINTER_VOLATILE,
    REDUCE_DECL_POINTER,

    //Array specifier
    EXPECT_ARRAY_CONSTANT,
    EXPECT_DECL_RSB,

    //Stmt list
    EXPECT_STMT_LIST,
    EXPECT_STMT,
    EXPECT_STMT_SC,
    EXPECT_NULL_STMT_SC,
    EXPECT_STMT_CRB,
    BASE_TYPE_CHECK_STMT,
    BASE_TYPE_CHECK_STMT_1,
    BASE_TYPE_CHECK_STMT_2,

    //Compound stmt
    EXPECT_COMPOUND_STMT,
    EXPECT_SINGLE_STMT,

    //If stmt
    EXPECT_STMT_IF,
    EXPECT_IF_LB,
    EXPECT_ELSE_IF,
    EXPECT_ELSE,
    REDUCE_IF_STMT,

    //Expr
    EXPECT_EXPR_UOP,
    EXPECT_EXPR_VAR,
    EXPECT_EXPR_CON,
    EXPECT_EXPR_LB,
    EXPECT_EXPR_POP,
    EXPECT_EXPR_BOP,
    EXPECT_EXPR_FN_LB,
    EXPECT_EXPR_RB,
    EXPECT_EXPR_SC,
    EXPECT_EXPR_UOP_S,
    EXPECT_EXPR_VAR_S,
    EXPECT_EXPR_CON_S,
    EXPECT_EXPR_LB_S,
    EXPECT_EXPR_FN_RB_S,
    
    //Notification states
    RETURN_STMT_REDUCE,
    STMT_LIST_REDUCE,
    LB_EXPR_REDUCE,
    FN_CALL_EXPR_REDUCE,
    STMT_REDUCE,
    IF_STMT_REDUCE,
    
    FN_DEF_REDUCE,
    DECL_LB_REDUCE,
    PARAM_LIST_REDUCE,

    //misc
    PARSER_INVALID,
    PARSER_ERROR,   
    PARSER_END
};

class state_machine {
    parser_states cur_state;
    std::vector<token>* token_stream;
    size_t token_idx;
    size_t num_states;

    bool advance_token;
    using fptr = bool (token::*)() const;
    using reducer = void (*)(state_machine*);
    using sptr = std::unique_ptr<ast> (*)(const token*);

    struct _state {
        parser_states state;
        parser_states next_state;
        parser_states alt_state;
        fptr pcheck;
        std::string name;
        bool shift;
        reducer reduce_this;
        bool fail_to_error;
        std::string error_string;
        parser_states push_state;
        bool is_special;
        sptr shiftfn;
    };

    std::vector<_state> state_path;

public:
    std::stack<std::unique_ptr<ast>> parser_stack;
    std::stack<parser_states> state_stack;
    
    state_machine();
    
    void set_token_stream(std::vector<token>&);


    token* fetch_token(); 
    std::unique_ptr<ast> fetch_parser_stack(); 
    const token* cur_token() const; 
    void stop_token_fetch();
    void switch_state(const parser_states new_state);
    void define_state(const std::string& state_name, parser_states next_state, state_machine::fptr pcheck, parser_states alt_state, bool shift = false, reducer reduce_this = nullptr, const char* error_string = nullptr, parser_states push_state = PARSER_INVALID);
    void define_shift_state(const std::string& state_name, parser_states next_state, state_machine::fptr pcheck, parser_states alt_state, sptr shiftfn, const char* error_string = nullptr, parser_states push_state = PARSER_INVALID);
    void define_special_state(const std::string& state_name, state_machine::fptr pcheck, reducer special_reduce, parser_states alt_state, const char* error_string = nullptr);
    void define_reduce_state(const std::string& state_name, parser_states next_state, reducer reduce_this); 
    void start();
};