#include <vector>
#include <list>
#include <optional>
#include "compiler/token.h"
#include "debug-api.h"

std::vector<token> tokens;  

enum lexer_states {
    LEXER_START,
    EXTENDED_OPERATOR_TOKEN,
    LOOKAHEAD_FOR_CONSTANT,
    EXPECTING_INTEGER_CONSTANT,
    EXPECTING_STRING_LITERAL,
    EXPECTING_STRING_CONSTANT,
    LOOKAHEAD_FOR_STRING_CONSTANT,
    LOOKAHEAD_FOR_WHITE_SPACE,
    LOOKAHEAD_FOR_TICK,
    EXPECTING_CHARACTER,
    EXPECTING_ESCAPED_CHARACTER,
    EXPECTING_TICK,
    LEXER_INVALID_TOKEN
};

#ifdef SIMDEBUG

std::vector<std::string> keywords_debug = { "INT", "CHAR", "VOID", "LONGLONG", "LONG", "SHORT", "UNSIGNED", "SIGNED", 
"CONST", "VOLATILE", "AUTO", "REGISTER", "EXTERN", "STATIC",
"RETURN", "WHILE", "DO", "FOR", "IF", "ELSE_IF", "ELSE", "BREAK", "CONTINUE" };

std::vector<std::string> op_debug = { "CLB", "CRB", "LB", "RB", "LSB", "RSB",
    "POINTER_TO", "DOT", "INCREMENT", "DECREMENT", "NOT", "BIT_NOT",
    "AMPER", "SIZEOF", "MUL", "DIV", "MODULO",
    "PLUS", "MINUS", "SHIFT_LEFT", "SHIFT_RIGHT", "GT", 
    "LT", "EQUAL_EQUAL", "NOT_EQUAL", "BIT_XOR", "BIT_OR",
    "AND", "OR", "EQUAL", "COMMA", "SEMICOLON"};

void print_token_list() {
    sim_log_debug("Printing tokens...");
    for(auto& tok: tokens) {
        tok.print();
    }
}

#endif

static std::optional<keyword_type> fetch_keyword(const std::string& literal) {
    
    std::optional<keyword_type> keyword;
    if(literal == "int") {
        keyword = TYPE_INT;
    }
    else if(literal == "char") {
        keyword = TYPE_CHAR;
    }
    else if(literal == "void") {
        keyword = TYPE_VOID;
    }
    else if(literal == "long") {
        keyword = TYPE_LONG;
    }
    else if(literal == "short") {
        keyword = TYPE_SHORT;
    }
    else if(literal == "unsigned") {
        keyword = TYPE_UNSIGNED;
    }
    else if(literal == "signed") {
        keyword = TYPE_SIGNED;
    }
    else if(literal == "const") {
        keyword = TYPE_CONST;
    }
    else if(literal == "volatile") {
        keyword = TYPE_VOLATILE;
    }
    else if(literal == "static") {
        keyword = TYPE_STATIC;
    }
    else if(literal == "auto") {
        keyword = TYPE_AUTO;
    }
    else if(literal == "extern") {
        keyword = TYPE_EXTERN;
    }
    else if(literal == "register") {
        keyword = TYPE_REGISTER;
    }
    else if(literal == "return") {
        keyword = RETURN;
    }
    else if(literal == "while") {
        keyword = WHILE;
    }
    else if(literal == "do") {
        keyword = DO;
    }
    else if(literal == "for") {
        keyword = FOR;
    }
    else if(literal == "if") {
        keyword = IF;
    }
    else if(literal == "else") {
        keyword = ELSE;
    }
    else if(literal == "break") {
        keyword = BREAK;
    }
    else if(literal == "continue") {
        keyword = CONTINUE;
    }

    return keyword;
}

static std::optional<char> fetch_escape_character(char ch) {
    
    std::optional<char> es_ch;
    switch (ch) {
        case 't': es_ch = '\t'; break;
        case 'r': es_ch = '\r'; break;
        case 'n': es_ch = '\n'; break;
        case 'b': es_ch = '\b'; break;
        case '0': es_ch = '\0'; break;
        case 'a': es_ch = '\a';
    }
    
    return es_ch;
}

static inline bool is_extended_operator(operator_type type) {
    switch (type) {
        case INCREMENT:
        case DECREMENT:
        case EQUAL_EQUAL:
        case NOT_EQUAL:
        case POINTER_TO:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
        case AND:
        case OR: return true;
    }

    return false;
}


void lex(const std::vector<char>& input) {
    lexer_states state = LEXER_START;
    tokens.clear();
        
    operator_type prev_op;
    size_t start_pos = 0, literal_count = 0;
    for(int i = 0; i < input.size(); i++) {
        char ch = input[i];
        operator_type op;

        if (state == EXPECTING_TICK) {
            if (ch == '\'') {
                sim_log_debug("Found closing tick. Switching back to LEXER_START");
                state = LEXER_START;
                continue;
            }
            else {
                sim_log_error("\' should be closed with just one character.");
            }
        }

        if (state == EXPECTING_ESCAPED_CHARACTER) {
            auto es_ch = fetch_escape_character(ch);
            if (es_ch) {
                sim_log_debug("Found escape character:{}", ch);
                tokens.push_back(token(CONSTANT, es_ch.value()));
                state = EXPECTING_TICK;
            }
            else {
                sim_log_error("Character {} is not a valid escape character", ch);
            }
        }

        if (state == EXPECTING_CHARACTER) {
            if (ch == '\\') {
                sim_log_debug("Found backslash. Switching state to EXPECTING_SINGLE_CHAR");
                state = EXPECTING_ESCAPED_CHARACTER;
            }
            else {
                tokens.push_back(token(CONSTANT, ch));
                state = EXPECTING_TICK;
            }
        }


        if(state == EXPECTING_STRING_CONSTANT) {
            if(ch == '\"') {
                std::string literal(input.begin()+start_pos+1, input.begin()+start_pos+literal_count+1);
                sim_log_debug("Pushing string constant:{} with literal_count: {}", literal, literal_count);
                tokens.push_back(token(CONSTANT, TOK_STRING, literal));
                state = LEXER_START;
                continue;
            }
            else
                literal_count++;
        }

        if(state == EXPECTING_INTEGER_CONSTANT) {
            if(isdigit(ch)) {
                literal_count++;
            }
            else if(isalpha(ch) || ch == '_') {
                sim_log_error("Variable names are not supposed to start with a digit.");
            }
            else {
                std::string num_literal(input.begin()+start_pos, input.begin()+start_pos+literal_count);
                tokens.push_back(token(CONSTANT, TOK_INT, num_literal));
                sim_log_debug("Pushed integer token:{}", num_literal);
                state = LEXER_START;
            }

        }

        if(state == EXPECTING_STRING_LITERAL) {
            //_ is also a valid character
            if(isalnum(ch) || ch == '_') {
                literal_count++;
            }
            else {
                std::string literal(input.begin()+start_pos, input.begin()+start_pos+literal_count);
                sim_log_debug("Identified literal:{} , literal_count:{}", literal, literal_count);
                auto key_type = fetch_keyword(literal);

                if(key_type) {
                    if(key_type.value() == IF) {
                        //This checks if previous token is an "else" and then combines it with the present "if" to create an "else_if" token
                        if (tokens.size() != 0 && tokens.back().is_keyword_else()) {
                            key_type = ELSE_IF;
                            std::get<keyword_type>(tokens.back().value) = key_type.value();
                        }
                        else
                            tokens.push_back(token(KEYWORD, key_type.value()));
                    }
                    else if(key_type.value() == TYPE_LONG) {
                        //This check if previous token is "long" and combines the current "long" with it to create "long long"
                        if (tokens.size() != 0 && tokens.back().is_keyword_long()) {
                            key_type = TYPE_LONGLONG;
                            std::get<keyword_type>(tokens.back().value) = key_type.value();
                        }
                        else
                            tokens.push_back(token(KEYWORD, key_type.value()));
                    }
                    else 
                        tokens.push_back(token(KEYWORD, key_type.value()));
                    
                    sim_log_debug("Pushing keyword token type:{}", keywords_debug[key_type.value()]);
                } 
                else {
                    sim_log_debug("Pushing identifier token:{}", literal);
                    tokens.push_back(token(IDENT, literal));
                }
                state = LEXER_START;
            }
        }

        if(state == EXTENDED_OPERATOR_TOKEN) {
            sim_log_debug("In state EXTENDED_OPERATOR_TOKEN.");
            op = prev_op;
            switch(ch) {
                case '+': {
                    if(prev_op == PLUS)
                        op = INCREMENT;
                    break;
                }
                case '-': {
                    if(prev_op == MINUS)
                        op = DECREMENT;
                    break;
                }
                case '&': {
                    if(prev_op == AMPER)
                        op = AND;
                    break;
                }
                case '|': {
                    if(prev_op == BIT_OR)
                        op = OR;
                    break;
                }
                case '>': {
                    if(prev_op == GT)
                        op = SHIFT_RIGHT;
                    else if(prev_op == MINUS)
                        op = POINTER_TO;
                    break;
                }
                case '<': {
                    if(prev_op == LT)
                        op = SHIFT_LEFT;
                    break;
                }
                case '=': {
                    if(prev_op == EQUAL)
                        op = EQUAL_EQUAL;
                    else if(prev_op == NOT)
                        op = NOT_EQUAL;
                    break;
                }
            }
            tokens.push_back(token(OPERATOR, op));
            sim_log_debug("Found operator_type:{}", op_debug[op]);
            state = LEXER_START;
            if (is_extended_operator(op))
                continue;
        }

        if(state == LEXER_START) {
            sim_log_debug("In state LEXER_START. Searching for operator token.");
            switch(ch) {
                case ';': op = SEMICOLON; break;
                case '{': op = CLB; break;
                case '}': op = CRB; break;
                case '~': op = BIT_XOR; break;
                case '*': op = MUL; break;
                case '/': op = DIV; break;
                case '%': op = MODULO; break;
                case '^': op = BIT_XOR; break;
                case '(': op = LB; break;
                case ')': op = RB; break;
                case '[': op = LSB; break;
                case ']': op = RSB; break;
                case ',': op = COMMA; break;
                case '!': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = NOT;
                    break;
                } 
                case '+': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = PLUS;
                    break;
                }
                case '-': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = MINUS;
                    break;
                }
                case '&': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = AMPER;
                    break;
                }
                case '|': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = BIT_OR;
                    break;
                }
                case '<': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = LT;
                    break;
                }
                case '>': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = GT;
                    break;
                }
                case '=': {
                    state = EXTENDED_OPERATOR_TOKEN;
                    prev_op = EQUAL;
                    break;
                }
                default: {
                    state = LOOKAHEAD_FOR_CONSTANT;
                }
            }

            if(state == LEXER_START) {
                sim_log_debug("Pushing operator_type:{}", op_debug[op]);
                tokens.push_back(token(OPERATOR, op));
            }
        }

        if(state == LOOKAHEAD_FOR_CONSTANT) {
            if(isdigit(ch)) {
                sim_log_debug("Found digit.. Switching state to EXPECTING_INTEGER_CONSTANT");
                start_pos = i;
                literal_count = 1;
                state = EXPECTING_INTEGER_CONSTANT;
            }
            else if(isalpha(ch) || ch == '_') {
                sim_log_debug("Found ascii character.. Switching state to EXPECTING_STRING_LITERAL");
                start_pos = i;
                literal_count = 1;
                state = EXPECTING_STRING_LITERAL;
            } 
            else 
                state = LOOKAHEAD_FOR_TICK;
        }

        if (state == LOOKAHEAD_FOR_TICK) {
            if (ch == '\'') {
                sim_log_debug("Found single tick");
                state = EXPECTING_CHARACTER;
            }
            else
                state = LOOKAHEAD_FOR_STRING_CONSTANT;
        }

        if(state == LOOKAHEAD_FOR_STRING_CONSTANT) {
            if(ch == '\"') {
                sim_log_debug("Switching state to EXPECTING_STRING_CONSTANT");
                start_pos = i;
                literal_count = 0;
                state = EXPECTING_STRING_CONSTANT;
            }
            else
                state = LOOKAHEAD_FOR_WHITE_SPACE;
        }

        if (state == LOOKAHEAD_FOR_WHITE_SPACE) {
            
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
                state = LEXER_START;
            else
                state = LEXER_INVALID_TOKEN;
        }

        if(state == LEXER_INVALID_TOKEN) {
            sim_log_error("Invalid token encountered:{}", ch);
        }
    }

#ifdef SIMDEBUG
    print_token_list();
#endif
    if(state != LEXER_START)
        sim_log_error("Lexical analysis failed")
}