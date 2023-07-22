#include "spdlog/fmt/fmt.h"
#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/ast.h"

#define AST_PRINT( msg, ... ) (sim_log_debug(std::string(level_space, ' ') + msg __VA_OPT__(,) __VA_ARGS__))

const size_t level_increment = 2;
size_t level_space = 0; 

void token::print() const {
    
    switch(type) {
        case KEYWORD:  AST_PRINT("type:KEYWORD keyword:{}", keywords_debug[std::get<keyword_type>(value)]); break;
        case IDENT: AST_PRINT("type:Identifier name:{}", std::get<std::string>(value)); break;
        case OPERATOR: AST_PRINT("type:Operator op:{}", op_debug[std::get<operator_type>(value)]); break;
        case CONSTANT: {
            if(sub_type == TOK_INT)
                AST_PRINT("type:INT_CONSTANT value:{}", std::get<size_t>(value));
            else if(sub_type == TOK_CHAR)
                AST_PRINT("type:CHAR_CONSTANT value:{}", std::get<char>(value));
            else
                AST_PRINT("type:STRING_CONSTANT value:{}", std::get<std::string>(value));
            break;
        }
        case NEWLINE: AST_PRINT("type:NEWLINE"); break;
        default: AST_PRINT("Invalid token??");
    }
    
}

void ast_fn_arg::print() {
    AST_PRINT("Node: fn_arg");
    decl_type->print();
    if(name)
        name->print();
}

void ast_fn_decl::print() {
    AST_PRINT("Node: fn_decl");
    ret_type->print();
    name->print();
    level_space += level_increment;
    children[0]->print();
    level_space -= level_increment;
}

void ast_decl::print() {
    AST_PRINT("Node: decl");
    identifier->print();
} 

void ast_decl_list::print() {
    AST_PRINT("Node: decl_list, num_nodes:{}", children.size());
    decl_type->print();
    level_space += level_increment;
    for(const auto& child: children) {
        child->print(); 
    }
    level_space -= level_increment;
}

static void print_ast_fn_arg_list(const ast* node) {
    AST_PRINT("Node: fn_arg_list, num_nodes:{}", node->children.size());
    size_t number = 0;
    level_space += level_increment;
    for(const auto& child: node->children) {
        child->print();
    }
    level_space -= level_increment;
}

static void print_ast_program(const ast* node) {
    AST_PRINT("Node: Program, num_nodes:{}", node->children.size());
    size_t number = 0;
    level_space += level_increment;
    for(const auto& child: node->children) {
        child->print();
    }
    level_space -= level_increment;
}

void ast::print() {
    switch(type) {
        case AST_TYPE::PROGRAM: print_ast_program(this); break;
        case AST_TYPE::FN_ARG_LIST: print_ast_fn_arg_list(this); break;
    }
}


