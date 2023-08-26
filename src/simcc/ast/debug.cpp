#include "spdlog/fmt/fmt.h"
#include "debug-api.h"
#include "compiler/token.h"
#include "compiler/ast.h"

#define AST_PRINT( msg, ... ) (sim_log_debug(std::string(level_space, ' ') + std::string(msg) __VA_OPT__(,) __VA_ARGS__))

const size_t level_increment = 2;
size_t level_space = 0; 

void token::print() const {
    
    switch(type) {
        case KEYWORD:  AST_PRINT("type:KEYWORD keyword:{}", keywords_debug[std::get<keyword_type>(value)]); break;
        case IDENT: AST_PRINT("type:Identifier name:{}", std::get<std::string>(value)); break;
        case OPERATOR: AST_PRINT("type:Operator op:{}", op_debug[std::get<operator_type>(value)]); break;
        case CONSTANT: {
            if(sub_type == TOK_INT)
                AST_PRINT("type:INT_CONSTANT value:{}", std::get<std::string>(value));
            else if(sub_type == TOK_CHAR)
                AST_PRINT("type:CHAR_CONSTANT value:{}", std::get<char>(value));
            else
                AST_PRINT("type:STRING_CONSTANT value:{}", std::get<std::string>(value));
            break;
        }
        default: AST_PRINT("Invalid token??");
    }
    
}

static void print_child_nodes(const std::deque<std::unique_ptr<ast>>& children) {
    for(const auto& child: children)
        child->print();
}

static void print_tokens(std::vector<const token*> args) {
    for(const auto& arg : args) {
        if(arg)
            arg->print();
    }
}

void ast_expr::print() {
    const static std::vector<std::string> expr_type_debug = {"Variable", "Constant", "Operator", "Punctuator", "Function call"}; 
    AST_PRINT("Expr node, type:{}", expr_type_debug[static_cast<int>(type)]);
    print_tokens({tok});
    level_space++;
    print_child_nodes(children);
    level_space--;
}

void ast_op::print() {
    static const char* op_type[] = {"unary", "binary", "postfix"};
    size_t op_index = is_postfix ? 2 : is_unary ? 0 : 1;
    AST_PRINT("Expr {} operator", op_type[op_index]);
    tok->print();
    level_space++;
    print_child_nodes(children);
    level_space--;
}

void ast_token::print() {
    tok->print();
}

void ast_decl::print() {
    AST_PRINT("Node: decl");
    print_tokens({ ident });
    level_space++;
    print_child_nodes(children);
    level_space--;
} 

void ast_base_type::print() {
    AST_PRINT("Node: base type");
    
    print_tokens({base_type.storage_spec, base_type.type_spec, base_type.sign_qual,
    base_type.const_qual, base_type.vol_qual});

}

void ast_ptr_spec::print() {
    AST_PRINT("Node: ptr specifier");

    print_tokens({pointer, const_qual, vol_qual});
}

void ast_array_spec::print() {
    AST_PRINT("Node: array specifier");

    constant->print();
}

void ast::print_ast_list(std::string_view msg) {
    AST_PRINT(msg);
    level_space++;
    print_child_nodes(children);
    level_space--;
}

void ast::print() {
    switch(type) {
        case AST_TYPE::PROGRAM: print_ast_list(fmt::format("Node: Program, num_nodes:{}", children.size())); break;
        case AST_TYPE::DECL_LIST: print_ast_list(fmt::format("Node: Decl list, num_nodes:{}", children.size())); break;
        case AST_TYPE::RETURN: print_ast_list("Node: return stmt"); break;
        case AST_TYPE::NULL_STMT: print_ast_list("Node: null stmt"); break;
        case AST_TYPE::STMT_LIST: print_ast_list("Node: stmt list"); break;
        case AST_TYPE::EXPR_STMT: print_ast_list("Node: expr stmt"); break;
        case AST_TYPE::PTR_LIST: print_ast_list("Node: ptr list"); break;
        case AST_TYPE::ARRAY_SPEC_LIST: print_ast_list("Node: array list"); break;
        case AST_TYPE::PARAM_LIST: print_ast_list("Node: param list"); break;
    }
}


