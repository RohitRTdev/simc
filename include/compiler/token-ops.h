#pragma once

#include "compiler/token.h"

static inline token create_operator_token(operator_type op) {
    token tok;
    tok.type = OPERATOR;
    tok.value = op;

    return tok;
}

static inline token create_constant_token(size_t num) {
    token tok;
    tok.type = CONSTANT;
    tok.sub_type = TOK_INT;
    tok.value = num;

    return tok;
}

static inline token create_constant_token(const std::string& literal) {
    token tok;
    tok.type = CONSTANT;
    tok.sub_type = TOK_STRING;
    tok.value = literal;

    return tok;
}

static inline token create_constant_token(char character) {
    token tok;
    tok.type = CONSTANT;
    tok.sub_type = TOK_CHAR;
    tok.value = character;

    return tok;
}

static inline token create_ident_token(const std::string& literal) {
    token tok;
    tok.type = IDENT;
    tok.value = literal;

    return tok;
}

static inline token create_keyword_token(keyword_type keyword) {
    token tok;
    tok.type = KEYWORD;
    tok.value = keyword;

    return tok;
}

static inline token create_newline_token() {
    token tok;
    tok.type = NEWLINE;
    return tok;
}

static inline bool is_keyword_else(const token& tok) {
    return tok.type == KEYWORD && std::get<keyword_type>(tok.value) == ELSE;
}

static inline bool is_token_newline(const token& tok) {
    return tok.type == NEWLINE;
}
