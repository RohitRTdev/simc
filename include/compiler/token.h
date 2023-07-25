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
private:
    token_type type;
    token_sub_type sub_type;

    bool is_data_type() const;

public:
    std::variant<size_t, char, std::string, operator_type, keyword_type> value;

    token(token_type type);

    template<typename T>
    token(token_type _type, const T& val) : type(_type) {
        value = val;
    }

    token(token_type _type, const size_t& num) {
        type = CONSTANT;
        sub_type = TOK_INT;
        value = num;
    }

    token(token_type _type, const std::string& val) {
        type = _type;
        if (_type == CONSTANT)
            sub_type = TOK_STRING;
        value = val;
    }

    token(token_type _type, const char& val) {
        type = CONSTANT;
        sub_type = TOK_CHAR;
        value = val;
    }

    bool is_keyword_else() const;
    bool is_newline() const;
    bool is_identifier() const;
    bool is_keyword_data_type() const;
    bool is_operator_comma() const;
    bool is_operator_lb() const;
    bool is_operator_rb() const;
    bool is_operator_sc() const;
    bool is_operator_clb() const; 
    bool is_operator_crb() const;
    bool is_operator_eq() const; 
    bool is_keyword_return() const;
    bool is_constant() const;

#ifdef SIMDEBUG
    void print() const;
#endif

};

extern std::vector<token> tokens;

#ifdef SIMDEBUG
extern std::vector<std::string> keywords_debug;
extern std::vector<std::string> op_debug;
#endif