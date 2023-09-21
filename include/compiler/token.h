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
    TYPE_LONGLONG,
    TYPE_LONG,
    TYPE_SHORT,
    TYPE_UNSIGNED,
    TYPE_SIGNED,
    TYPE_CONST,
    TYPE_VOLATILE,
    TYPE_AUTO,
    TYPE_REGISTER,
    TYPE_EXTERN,
    TYPE_STATIC,
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

    bool is_keyword_data_type() const;

public:
    std::variant<char, std::string, operator_type, keyword_type> value;
    
    template<typename T>
    token(token_type _type, const T& val) : type(_type) {
        value = val;
    }

    token(token_type _type, token_sub_type _sub_type, const std::string& num) {
        type = CONSTANT;
        sub_type = _sub_type;
        value = num;
    }

    token(token_type _type, const char& val) {
        type = CONSTANT;
        sub_type = TOK_CHAR;
        value = val;
    }

    bool is_keyword_else() const;
    bool is_keyword_return() const;
    bool is_keyword_long() const;
    bool is_keyword_auto() const;
    bool is_keyword_extern() const;
    bool is_keyword_static() const;
    bool is_keyword_register() const;
    bool is_keyword_const() const;
    bool is_keyword_volatile() const;
    bool is_keyword_signed() const;
    bool is_keyword_unsigned() const;
    bool is_keyword_char() const;
    bool is_keyword_void() const;
    bool is_keyword_if() const;
    bool is_keyword_else_if() const;
    bool is_keyword_while() const;
    bool is_keyword_break() const;
    bool is_keyword_continue() const;
    bool is_operator_comma() const;
    bool is_operator_lb() const;
    bool is_operator_rb() const;
    bool is_operator_sc() const;
    bool is_operator_clb() const; 
    bool is_operator_crb() const;
    bool is_operator_lsb() const;
    bool is_operator_rsb() const;
    bool is_operator_eq() const; 
    bool is_operator_plus() const;
    bool is_operator_star() const;
    bool is_data_type() const;
    bool is_storage_specifier() const;
    bool is_type_qualifier() const;
    bool is_identifier() const;
    bool is_constant() const;
    bool is_integer_constant() const;
    bool is_char_constant() const;
    bool is_string_constant() const;
    bool is_unary_operator() const; 
    bool is_binary_operator() const; 
    bool is_postfix_operator() const; 

#ifdef SIMDEBUG
    void print() const;
#endif

};

extern std::vector<token> tokens;

#ifdef SIMDEBUG
extern std::vector<std::string> keywords_debug;
extern std::vector<std::string> op_debug;
#endif
