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

static inline bool is_token_identifier(const token& tok) {
    return tok.type == IDENT;
}

static inline bool is_data_type(keyword_type type) {
    switch(type) {
        case TYPE_INT:
        case TYPE_CHAR:
        case TYPE_VOID: return true;
    }

    return false;
}

static inline bool is_token_data_type(const token& tok) {
    return tok.type == KEYWORD && is_data_type(std::get<keyword_type>(tok.value));
}

static inline bool is_token_operator_comma(const token& tok) {
    return tok.type == OPERATOR && std::get<operator_type>(tok.value) == COMMA;
}

static inline bool is_token_operator_lb(const token& tok) {
    return tok.type == OPERATOR && std::get<operator_type>(tok.value) == LB;
}

static inline bool is_token_operator_sc(const token& tok) {
    return tok.type == OPERATOR && std::get<operator_type>(tok.value) == SEMICOLON;
}

static inline bool is_token_operator_rb(const token& tok) {
    return tok.type == OPERATOR && std::get<operator_type>(tok.value) == RB;
}