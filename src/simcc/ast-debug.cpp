#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/ast.h"

void ast_fn_arg::print() {
    sim_log_debug("Node: fn_arg");
    decl_type->print();
    if(name)
        name->print();
}

void ast_fn_decl::print() {
    sim_log_debug("Node: fn_decl");
    ret_type->print();
    name->print();
    children[0]->print();
}

void ast_decl::print() {
    sim_log_debug("Node: decl");
    identifier->print();
} 

void ast_decl_list::print() {
    sim_log_debug("Node: decl_list, num_nodes:{}", children.size());
    decl_type->print();
    size_t number = 0;
    for(const auto& child: children) {
        sim_log_debug("Enumerating child node:{}", number++);
        child->print(); 
    }
}

static void print_ast_fn_arg_list(const ast* node) {
    sim_log_debug("Node: fn_arg_list, num_nodes:{}", node->children.size());
    size_t number = 0;
    for(const auto& child: node->children) {
        sim_log_debug("Enumerating child node:{}", number++);
        child->print();
    }
}

static void print_ast_program(const ast* node) {
    sim_log_debug("Node: Program, num_nodes:{}", node->children.size());
    size_t number = 0;
    for(const auto& child: node->children) {
        sim_log_debug("Enumerating child node:{}", number++);
        child->print();
    }
}

void ast::print() {
    switch(type) {
        case AST_TYPE::PROGRAM: print_ast_program(this); break;
        case AST_TYPE::FN_ARG_LIST: print_ast_fn_arg_list(this); break;
    }
}


