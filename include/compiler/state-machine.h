#pragma once

#include <vector>
#include <memory>
#include <stack>
#include "compiler/token.h"
#include "compiler/ast.h"


enum parser_states {
    //Common
    PARSER_START = 0,
    EXPECT_NEWLINE,
    EXPECT_IDENT,

    //Fn arg
    EXPECT_LB,
    EXPECT_FN_ARG_LIST_START,
    EXPECT_FN_ARG_LIST_IDENT,
    EXPECT_FN_ARG_LIST_COMMA,
    EXPECT_FN_ARG_LIST,
    FN_ARG_REDUCE,
    EXPECT_RB,
    EXPECT_SC, 
    EXPECT_FN_CLB,
    FN_DEF_REDUCE,

    //Decl list
    EXPECT_DECL_COMMA_OR_SC,
    EXPECT_DECL_COMMA,
    DECL_REDUCE,
    EXPECT_DECL_SC,
    EXPECT_DECL_IDENT,
    EXPECT_DECL_EQ,
    EXPECT_DECL_VAR,
    EXPECT_DECL_CON,

    //Fn body
    EXPECT_STMT_LIST,
    EXPECT_STMT_NEWLINE,

    //Stmt list
    EXPECT_STMT,
    EXPECT_STMT_SC,
    EXPECT_NULL_STMT_SC,
    EXPECT_CRB,
    
    //Notification states
    PARSER_STMT_RETURN,

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
    };

    std::vector<_state> state_path;

public:
    std::stack<std::unique_ptr<ast>> parser_stack;
    std::stack<parser_states> state_stack;
    
    state_machine();
    
    void set_token_stream(std::vector<token>&);


    token* fetch_token(); 
    std::unique_ptr<ast> fetch_parser_stack(); 
    
    void stop_token_fetch();
    void switch_state(const parser_states new_state);
    void define_state(const std::string& state_name, parser_states next_state, state_machine::fptr pcheck, parser_states alt_state, bool shift = false, reducer reduce_this = nullptr, const char* error_string = nullptr, parser_states push_state = PARSER_INVALID);
    void define_special_state(const std::string& state_name, state_machine::fptr pcheck, reducer special_reduce, parser_states alt_state, const char* error_string = nullptr);
    void define_reduce_state(const std::string& state_name, parser_states next_state, reducer reduce_this); 
    void start();
};