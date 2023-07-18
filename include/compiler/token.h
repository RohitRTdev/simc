#pragma once
#include <vector>
#include <string>
#include <variant>

enum token_type{
    KEYWORD,
    IDENT,  
    OPERATOR,
    CONSTANT,
    NEWLINE
};

enum token_sub_type {
    TOK_STRING,
    TOK_INT,
    TOK_CHAR
};

enum keyword_type {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_VOID,
    RETURN,
    WHILE,
    DO,
    FOR,
    IF,
    ELSE_IF,
    ELSE,
    BREAK,
    CONTINUE
};

enum operator_type {
    CLB,
    CRB,
    LB,
    RB,
    LSB,
    RSB,
    POINTER_TO,
    DOT,
    INCREMENT,
    DECREMENT,
    NOT,
    BIT_NOT,
    AMPER,
    SIZEOF,
    MUL,
    DIV,
    MODULO,
    PLUS,
    MINUS,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    GT,
    LT,
    EQUAL_EQUAL,
    NOT_EQUAL,
    BIT_XOR,
    BIT_OR,
    AND,
    OR,
    EQUAL,
    COMMA,
    SEMICOLON
};

struct token {
    token_type type;
    token_sub_type sub_type;

    std::variant<size_t, char, std::string, operator_type, keyword_type> value;

//    union {
//        size_t number;
//        char character;
//        operator_type op;
//        keyword_type keyword;
//    } value;

};

extern std::vector<token> tokens;

void lex(const std::vector<char>& input);
