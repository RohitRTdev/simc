#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/ast.h"

static void print_ast_fn_arg(const ast_fn_arg* node) {
    sim_log_debug("Node: fn_arg");
    print_token(*node->decl_type);
    if(node->name)
        print_token(*node->name);
}

static void print_ast_fn_arg_list(const ast* node) {
    sim_log_debug("Node: fn_arg_list, num_nodes:{}", node->children.size());
    size_t number = 0;
    for(const auto& child: node->children) {
        sim_log_debug("Enumerating child node:{}", number++);
        print_ast(child);
    }
}

static void print_ast_fn_decl(const ast_fn_decl* node) {
    sim_log_debug("Node: fn_decl");
    print_token(*node->ret_type);
    print_token(*node->name);
    print_ast(node->children[0]);
}

static void print_ast_decl(const ast_decl* node) {
    sim_log_debug("Node: decl");
    print_token(*node->identifier);
} 

static void print_ast_decl_list(const ast_decl_list* node) {
    sim_log_debug("Node: decl_list, num_nodes:{}", node->children.size());
    print_token(*node->decl_type);
    size_t number = 0;
    for(const auto& child: node->children) {
        sim_log_debug("Enumerating child node:{}", number++);
        print_ast(child);
    }
}

static void print_ast_program(const ast* node) {
    sim_log_debug("Node: Program, num_nodes:{}", node->children.size());
    size_t number = 0;
    for(const auto& child: node->children) {
        sim_log_debug("Enumerating child node:{}", number++);
        print_ast(child);
    }
}

void print_ast(const std::unique_ptr<ast>& node) {
    switch(node->type) {
        case AST_TYPE::PROGRAM: print_ast_program(node.get()); break;
        case AST_TYPE::DECL_LIST: print_ast_decl_list(static_cast<ast_decl_list*>(node.get())); break;
        case AST_TYPE::DECL: print_ast_decl(static_cast<ast_decl*>(node.get())); break;
        case AST_TYPE::FN_DECL: print_ast_fn_decl(static_cast<ast_fn_decl*>(node.get())); break;
        case AST_TYPE::FN_ARG_LIST: print_ast_fn_arg_list(node.get()); break;
        case AST_TYPE::FN_ARG: print_ast_fn_arg(static_cast<ast_fn_arg*>(node.get())); break;
    }
}


