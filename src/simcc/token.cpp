#include "compiler/token.h"
#include "debug-api.h"

token::token(token_type _type) : type(_type) {
}

bool token::is_keyword_else() const {
    return type == KEYWORD && std::get<keyword_type>(value) == ELSE;
}

bool token::is_newline() const {
    return type == NEWLINE;
}

bool token::is_identifier() const {
    return type == IDENT;
}

bool token::is_data_type() const {
    keyword_type type = std::get<keyword_type>(value);
    switch(type) {
        case TYPE_INT:
        case TYPE_CHAR:
        case TYPE_VOID: return true;
    }

    return false;
}

bool token::is_keyword_data_type() const {
    return type == KEYWORD && is_data_type();
}

bool token::is_operator_comma() const {
    return type == OPERATOR && std::get<operator_type>(value) == COMMA;
}

bool token::is_operator_lb() const {
    return type == OPERATOR && std::get<operator_type>(value) == LB;
}

bool token::is_operator_sc() const {
    return type == OPERATOR && std::get<operator_type>(value) == SEMICOLON;
}

bool token::is_operator_rb() const {
    return type == OPERATOR && std::get<operator_type>(value) == RB;
}

bool token::is_operator_clb() const {
    return type == OPERATOR && std::get<operator_type>(value) == CLB;
}

bool token::is_operator_crb() const {
    return type == OPERATOR && std::get<operator_type>(value) == CRB;
}

bool token::is_keyword_return() const {
    return type == KEYWORD && std::get<keyword_type>(value) == RETURN;
}

bool token::is_constant() const {
    return type == CONSTANT;
}

bool token::is_operator_eq() const {
    return type == OPERATOR && std::get<operator_type>(value) == EQUAL;
}

bool token::is_unary_operator() const {
    if(type != OPERATOR)
        return false;
    
    switch(std::get<operator_type>(value)) {
        case PLUS:
        case MINUS:
        case AMPER:
        case NOT:
        case BIT_NOT:
        case MUL:
        case INCREMENT:
        case DECREMENT: return true;
    }

    return false;
}

bool token::is_binary_operator() const {
    if(type != OPERATOR)
        return false;
    
    switch(std::get<operator_type>(value)) {
        case PLUS:
        case MINUS:
        case AMPER:
        case DIV:
        case MODULO:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
        case GT:
        case LT:
        case EQUAL_EQUAL:
        case NOT_EQUAL:
        case BIT_XOR:
        case BIT_OR:
        case AND:
        case OR:
        case EQUAL:
        case MUL: return true;
    }

    return false;
}

bool token::is_postfix_operator() const {
    if(type != OPERATOR)
        return false;
    
    switch(std::get<operator_type>(value)) {
        case INCREMENT:
        case DECREMENT: return true;
    }

    return false;
}

