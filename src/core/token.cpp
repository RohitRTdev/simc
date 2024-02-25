#include "core/token.h"
#include "common/diag.h"
#include "debug-api.h"

#ifdef MODSIMCC
diag token::global_diag_inst;

void token::print_error() const {
    global_diag_inst.print_error(position);
}
#endif

bool token::is_keyword_else() const {
    return type == KEYWORD && std::get<keyword_type>(value) == ELSE;
}

bool token::is_keyword_if() const {
    return type == KEYWORD && std::get<keyword_type>(value) == IF;
}

bool token::is_keyword_while() const {
    return type == KEYWORD && std::get<keyword_type>(value) == WHILE;
}

bool token::is_keyword_break() const {
    return type == KEYWORD && std::get<keyword_type>(value) == BREAK;
}

bool token::is_keyword_continue() const {
    return type == KEYWORD && std::get<keyword_type>(value) == CONTINUE;
}

bool token::is_keyword_else_if() const {
    return type == KEYWORD && std::get<keyword_type>(value) == ELSE_IF;
}

bool token::is_identifier() const {
    return type == IDENT;
}

bool token::is_keyword_data_type() const {
    keyword_type type = std::get<keyword_type>(value);
    switch(type) {
        case TYPE_INT:
        case TYPE_CHAR:
        case TYPE_VOID:
        case TYPE_LONG: 
        case TYPE_LONGLONG:
        case TYPE_SHORT:
        case TYPE_SIGNED:
        case TYPE_UNSIGNED: return true;     
    }

    return false;
}

bool token::is_type_qualifier() const {
    if(type != KEYWORD)
        return false;

    switch(std::get<keyword_type>(value)) {
        case TYPE_CONST:
        case TYPE_VOLATILE: return true;
    }

    return false;
}

bool token::is_storage_specifier() const {
    if(type != KEYWORD)
        return false;
    
    switch(std::get<keyword_type>(value)) {
        case TYPE_AUTO:
        case TYPE_REGISTER:
        case TYPE_EXTERN:
        case TYPE_STATIC: return true;
    }

    return false;
}

bool token::is_data_type() const {
    return type == KEYWORD && is_keyword_data_type();
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

bool token::is_operator_lsb() const {
    return type == OPERATOR && std::get<operator_type>(value) == LSB;
}

bool token::is_operator_rsb() const {
    return type == OPERATOR && std::get<operator_type>(value) == RSB;
}

bool token::is_keyword_return() const {
    return type == KEYWORD && std::get<keyword_type>(value) == RETURN;
}

bool token::is_constant() const {
    return type == CONSTANT;
}

bool token::is_integer_constant() const {
    return is_constant() && sub_type == TOK_INT;
}

bool token::is_char_constant() const {
    return is_constant() && sub_type == TOK_CHAR;
}

bool token::is_string_constant() const {
    return is_constant() && sub_type == TOK_STRING;
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
        case MUL: 
        case COMMA: return true;
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

bool token::is_operator_plus() const {
    return type == OPERATOR && std::get<operator_type>(value) == PLUS;
}

bool token::is_operator_star() const {
    return type == OPERATOR && std::get<operator_type>(value) == MUL;
}

bool token::is_keyword_long() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_LONG;
}

bool token::is_keyword_const() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_CONST;
}

bool token::is_keyword_volatile() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_VOLATILE;
}

bool token::is_keyword_auto() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_AUTO;
}

bool token::is_keyword_register() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_REGISTER;
}

bool token::is_keyword_extern() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_EXTERN;
}

bool token::is_keyword_static() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_STATIC;
}

bool token::is_keyword_signed() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_SIGNED;
}

bool token::is_keyword_unsigned() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_UNSIGNED;
}

bool token::is_keyword_char() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_CHAR;
}

bool token::is_keyword_void() const {
    return type == KEYWORD && std::get<keyword_type>(value) == TYPE_VOID;
}

#ifdef SIMDEBUG

std::vector<std::string> keywords_debug = { "INT", "CHAR", "VOID", "LONGLONG", "LONG", "SHORT", "UNSIGNED", "SIGNED",
"CONST", "VOLATILE", "AUTO", "REGISTER", "EXTERN", "STATIC",
"RETURN", "WHILE", "DO", "FOR", "IF", "ELSE_IF", "ELSE", "BREAK", "CONTINUE" };

std::vector<std::string> op_debug = { "CLB", "CRB", "LB", "RB", "LSB", "RSB",
    "POINTER_TO", "DOT", "INCREMENT", "DECREMENT", "NOT", "BIT_NOT",
    "AMPER", "SIZEOF", "MUL", "DIV", "MODULO",
    "PLUS", "MINUS", "SHIFT_LEFT", "SHIFT_RIGHT", "GT",
    "LT", "EQUAL_EQUAL", "NOT_EQUAL", "BIT_XOR", "BIT_OR",
    "AND", "OR", "EQUAL", "COMMA", "SEMICOLON" };

void print_token_list(const std::vector<token>& tokens) {
    sim_log_debug("Printing tokens...");
    for (auto& tok : tokens) {
        tok.print();
    }
}

#endif