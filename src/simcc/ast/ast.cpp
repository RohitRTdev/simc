#include "compiler/ast.h"
#include "debug-api.h"

ast::ast(AST_TYPE _type) : type(_type) 
{}

ast_token::ast_token(const token* _tok) : ast(AST_TYPE::TOKEN), tok(_tok)
{}

ast_token::ast_token(const token* _tok, bool is_expr) : ast(AST_TYPE::EXPR), tok(_tok)
{}

ast_base_type::ast_base_type(const token* storage_spec, const token* type_spec,
const token* const_qual, const token* vol_qual, const token* sign_qual) : ast(AST_TYPE::BASE_TYPE),
base_type{storage_spec, type_spec, const_qual, vol_qual, sign_qual} {

}

ast_ptr_spec::ast_ptr_spec(const token* _pointer, const token* _const_qual, const token* _vol_qual) : ast(AST_TYPE::PTR_SPEC), 
pointer(_pointer), const_qual(_const_qual), vol_qual(_vol_qual) {
}

ast_array_spec::ast_array_spec(const token* _constant) : ast(AST_TYPE::ARRAY_SPEC), constant(_constant) {

}

ast_decl::ast_decl(const token* _ident) : ast(AST_TYPE::DECL), ident(_ident) {

}

void ast::attach_node(std::unique_ptr<ast> node) {
    children.push_front(std::move(node));
}

void ast::attach_back(std::unique_ptr<ast> node) {
    children.push_back(std::move(node));
} 

std::unique_ptr<ast> ast::remove_node() {
    auto node = std::move(children.back());
    children.pop_back();
    return node;
}

bool ast::is_decl_list() const {
    return type == AST_TYPE::DECL_LIST;
}

bool ast::is_decl() const {
    return type == AST_TYPE::DECL;
} 

bool ast::is_stmt() const {
    switch(type) {
        case AST_TYPE::RETURN:
        case AST_TYPE::NULL_STMT: 
        case AST_TYPE::DECL_STMT:
        case AST_TYPE::EXPR_STMT: 
        case AST_TYPE::IF: 
        case AST_TYPE::WHILE: return true;
    }

    return false;
}

bool ast::is_stmt_list() const {
    return type == AST_TYPE::STMT_LIST;
}

bool ast::is_while() const {
    return type == AST_TYPE::WHILE;
}

bool ast::is_if() const {
    return type == AST_TYPE::IF;
}

bool ast::is_else_if() const {
    return type == AST_TYPE::ELSE_IF;
}

bool ast::is_else() const {
    return type == AST_TYPE::ELSE;
}

bool ast::is_token() const {
    return type == AST_TYPE::TOKEN;
}

bool ast::is_expr() const {
    return type == AST_TYPE::EXPR;
}

bool ast::is_prog() const {
    return type == AST_TYPE::PROGRAM;
}

bool ast::is_fn_def() const {
    return type == AST_TYPE::FN_DEF;
}

bool ast::is_null_stmt() const {
    return type == AST_TYPE::NULL_STMT;
}

bool ast::is_ret_stmt() const {
    return type == AST_TYPE::RETURN;
}

bool ast::is_expr_stmt() const {
    return type == AST_TYPE::EXPR_STMT;
}

bool ast::is_decl_stmt() const {
    return type == AST_TYPE::DECL_STMT;
}

bool ast::is_pointer_list() const {
    return type == AST_TYPE::PTR_LIST;
}

bool ast::is_array_specifier_list() const {
    return type == AST_TYPE::ARRAY_SPEC_LIST;
}

bool ast::is_base_type() const {
    return type == AST_TYPE::BASE_TYPE;
}

bool ast::is_param_list() const {
    return type == AST_TYPE::PARAM_LIST;
}

bool ast::is_break_stmt() const {
    return type == AST_TYPE::BREAK;
}

bool ast::is_continue_stmt() const {
    return type == AST_TYPE::CONTINUE;
}

bool ast_ptr_spec::print_error() const {
    pointer->print_error();
    return true;
}

bool ast_array_spec::print_error() const {
    constant->print_error();
    return true;
}

bool ast_decl::print_error() const {
    if(ident) {
        ident->print_error();
        return true;
    }

    for(const auto& child: children) {
        if(child->print_error()) {
            return true;
        }
    }

    return false;
}

bool ast_token::print_error() const {
    CRITICAL_ASSERT(tok, "ast_token::print_error() called on null token");
    tok->print_error();
    return true;
}

bool ast_fn_call::print_error() const {
    tok->print_error();
    return true;
}

bool ast_base_type::print_error() const {
    base_type.print_error();
    return true;
}

bool ast::print_error() const {
    switch(type) {
        case AST_TYPE::DECL_LIST: 
        case AST_TYPE::PTR_LIST: 
        case AST_TYPE::ARRAY_SPEC_LIST: {
            children[0]->print_error();
            break;
        }
        default: {
            if(key_token) {
                key_token->print_error();
                break;
            }
            CRITICAL_ASSERT_NOW("ast::print_error() called on invalid ast node of type:{}", static_cast<int>(type));
        }
    }

    return true;
}
