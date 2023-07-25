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

static void print_child_nodes(const std::deque<std::unique_ptr<ast>>& children) {
    for(const auto& child: children)
        child->print();
}

void ast_token::print() {
    tok->print();
}

void print_ast_fn_arg(const ast* node) {
    AST_PRINT("Node: fn_arg");
    level_space++;
    print_child_nodes(node->children);
    level_space--;
}

void print_ast_fn_decl(const ast* node) {
    AST_PRINT("Node: fn_decl");
    level_space += level_increment;
    print_child_nodes(node->children);
    level_space -= level_increment;
}

void print_ast_decl(const ast* node) {
    AST_PRINT("Node: decl");
    level_space++;
    print_child_nodes(node->children);
    level_space--;
} 

static void print_ast_decl_list(const ast* node) {
    AST_PRINT("Node: decl_list, num_nodes:{}", node->children.size());
    level_space += level_increment;
    print_child_nodes(node->children);
    level_space -= level_increment;
}

static void print_ast_fn_arg_list(const ast* node) {
    AST_PRINT("Node: fn_arg_list, num_nodes:{}", node->children.size());
    size_t number = 0;
    level_space += level_increment;
    print_child_nodes(node->children);
    level_space -= level_increment;
}

static void print_ast_program(const ast* node) {
    AST_PRINT("Node: Program, num_nodes:{}", node->children.size());
    size_t number = 0;
    level_space += level_increment;
    print_child_nodes(node->children);
    level_space -= level_increment;
}

static void print_ast_return(const ast* node) {
    AST_PRINT("Node: return statement");
}

static void print_ast_null_stmt(const ast* node) {
    AST_PRINT("Node: Null statement");
}

static void print_ast_fn_def(const ast* node) {
    AST_PRINT("Node: Fn definition");
    level_space++;
    print_child_nodes(node->children);
    level_space--;
}

static void print_ast_stmt_list(const ast* node) {
    AST_PRINT("Node: Stmt List, statements:{}", node->children.size());
    level_space++;
    print_child_nodes(node->children);
    level_space--;
}

void ast::print() {
    switch(type) {
        case AST_TYPE::PROGRAM: print_ast_program(this); break;
        case AST_TYPE::FN_ARG_LIST: print_ast_fn_arg_list(this); break;
        case AST_TYPE::DECL: print_ast_decl(this); break;
        case AST_TYPE::DECL_LIST: print_ast_decl_list(this); break;
        case AST_TYPE::FN_ARG: print_ast_fn_arg(this); break;
        case AST_TYPE::FN_DECL: print_ast_fn_decl(this); break;
        case AST_TYPE::RETURN: print_ast_return(this); break;
        case AST_TYPE::NULL_STMT: print_ast_null_stmt(this); break;
        case AST_TYPE::STMT_LIST: print_ast_stmt_list(this); break;
        case AST_TYPE::FN_DEF: print_ast_fn_def(this); break;
    }
}


