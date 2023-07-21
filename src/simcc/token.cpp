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

#ifdef SIMDEBUG
void token::print() const {
    
    switch(type) {
        case KEYWORD:  sim_log_debug("type:KEYWORD keyword:{}", keywords_debug[std::get<keyword_type>(value)]); break;
        case IDENT: sim_log_debug("type:Identifier name:{}", std::get<std::string>(value)); break;
        case OPERATOR: sim_log_debug("type:Operator op:{}", op_debug[std::get<operator_type>(value)]); break;
        case CONSTANT: {
            if(sub_type == TOK_INT)
                sim_log_debug("type:INT_CONSTANT value:{}", std::get<size_t>(value));
            else if(sub_type == TOK_CHAR)
                sim_log_debug("type:CHAR_CONSTANT value:{}", std::get<char>(value));
            else
                sim_log_debug("type:STRING_CONSTANT value:{}", std::get<std::string>(value));
            break;
        }
        case NEWLINE: sim_log_debug("type:NEWLINE"); break;
        default: sim_log_debug("Invalid token??");
    }
    
}
#endif