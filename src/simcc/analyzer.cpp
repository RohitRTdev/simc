#include "compiler/ast.h"
#include "compiler/scope.h"
#include "compiler/ast-ops.h"
#include "compiler/compile.h"
#include "debug-api.h"
scope* current_scope = nullptr;


static void eval_gdecl_list(std::unique_ptr<ast> decl_list, std::shared_ptr<Itranslation> tu) {
    
    auto type = std::move(decl_list->children[0]);
    c_type _type;
    switch(std::get<keyword_type>(cast_to_ast_token(type)->tok->value)) {
        case TYPE_INT: _type = C_INT; break;
        default: sim_log_error("Types other than int not supported right now");
    }

    for(int i = 1; i < decl_list->children.size(); i++) {
        auto decl = std::move(decl_list->children[i]);
        auto ident = std::move(decl->children[0]);
        auto& name = std::get<std::string>(cast_to_ast_token(ident)->tok->value);

        if(decl->children.size() > 1) {
            auto value = std::move(decl->children[1]);
            auto& val = std::get<std::string>(cast_to_ast_token(value)->tok->value);

            int id = tu->declare_global_variable(name, _type, val);

            current_scope->add_variable(id, name, _type, val);
        } 
        else {
            int id = tu->declare_global_variable(name, _type);

            current_scope->add_variable(id, name, _type);
        }
    }

}

static void setup_fn_decl(std::unique_ptr<ast> ret_type, std::unique_ptr<ast> name, std::unique_ptr<ast> arg_list, std::shared_ptr<Itranslation> tu) {

    std::vector<c_type> formal_arg_list(arg_list->children.size());
    size_t arg_idx = 0;
    for(auto& arg: arg_list->children) {
        auto& type = arg->children[0];
        switch(std::get<keyword_type>(cast_to_ast_token(type)->tok->value)) {
            case TYPE_INT: formal_arg_list[arg_idx++] = C_INT; break;
            default: sim_log_error("Types other than int not supported right now");
        }
    }
    
    current_scope->add_function(cast_to_ast_token(name)->tok, cast_to_ast_token(ret_type)->tok, formal_arg_list);

}

static void eval_fn_decl(std::unique_ptr<ast> fn_decl, std::shared_ptr<Itranslation> tu) {
    setup_fn_decl(std::move(fn_decl->children[0]), std::move(fn_decl->children[1]), std::move(fn_decl->children[2]), tu);
}

static void eval_fn_def(std::unique_ptr<ast> fn_def, std::shared_ptr<Itranslation> tu) {
    const auto& fn_name = std::get<std::string>(cast_to_ast_token(fn_def->children[1])->tok->value); 
    setup_fn_decl(std::move(fn_def->children[0]), std::move(fn_def->children[1]), std::move(fn_def->children[2]), tu);

    current_scope->add_function_definition(fn_name, tu->add_function(fn_name));
}

void eval(std::unique_ptr<ast> prog) {
    sim_log_debug("Starting evaluator...");
    CRITICAL_ASSERT(prog->is_prog(), "eval() called with non program node");

    current_scope = new scope();
    auto tu = std::shared_ptr<Itranslation>(create_translation_unit());

    for(auto& child: prog->children) {
        if(child->is_decl_list()) {
            eval_gdecl_list(std::move(child), tu);
        }
        else if(child->is_fn_decl()) {
            eval_fn_decl(std::move(child), tu);
        }
        else if(child->is_fn_def()) {
            eval_fn_def(std::move(child), tu);
        }
        else {
            CRITICAL_ASSERT_NOW("Invalid ast node found as child of program node");
        }
    }

    tu->generate_code();
    asm_code = tu->fetch_code();
}