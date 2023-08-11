#include <stack>
#include "compiler/ast.h"
#include "compiler/scope.h"
#include "compiler/ast-ops.h"
#include "compiler/compile.h"
#include "debug-api.h"
scope* current_scope = nullptr;


struct expr_result {
    c_type eval_type;
    int expr_id;
    bool is_lvalue;
    const token* constant;
    const token* variable;
};

static expr_result eval_expr(std::unique_ptr<ast> expr, Ifunc_translation* fn) {
    std::stack<expr_result> res_stack;
    std::stack<std::unique_ptr<ast>> op_stack;

    auto opt_release_stack = [&] {
        if(op_stack.empty()) {
            expr.reset();
        }
        else {
            expr = std::move(op_stack.top());
            op_stack.pop();
        }
    };

    auto pop_res_stack = [&] {
        auto _val = res_stack.top();
        res_stack.pop();
        return _val;
    };

    sim_log_debug("Evaluating expression");
    while(!op_stack.empty() || expr) {
        auto _expr = cast_to_ast_expr(expr);
#ifdef SIMDEBUG
        _expr->tok->print();
#endif
        if(_expr->is_con()) {
            CRITICAL_ASSERT(_expr->tok->is_integer_constant(), "Only integer constants supported now");
            expr_result tmp{C_INT, 0, false, _expr->tok, nullptr};

            res_stack.push(tmp);
            opt_release_stack();
        }
        else if(_expr->is_var()) {
            auto& var = current_scope->fetch_var_info(std::get<std::string>(_expr->tok->value));
            CRITICAL_ASSERT(var.var.type == C_INT, "Only integer type variables supported now");

            int local_id = var.var_id;
            if(var.is_global) {
                local_id = fn->fetch_global_var_int(var.var_id);
            }

            expr_result tmp{C_INT, local_id, true, nullptr, _expr->tok};
            res_stack.push(tmp);

            opt_release_stack();
        }
        else if(_expr->is_operator()) {
            auto _op = cast_to_ast_op(expr);
            CRITICAL_ASSERT(!_op->is_unary && _op->tok->is_operator_plus(), "Only binary '+' operator supported in expression right now");


            if(!expr->children.size()) {
                auto res1 = pop_res_stack();
                auto res2 = pop_res_stack();

                int res_id = 0;

                CRITICAL_ASSERT(!(res1.constant && res2.constant), "Both operands being constants is not supported right now");

                if(res1.constant || res2.constant) {
                    std::string_view const_str;
                    if(res1.constant) {
                        const_str = std::get<std::string>(res1.constant->value);
                        res_id = fn->add_int_c(res2.expr_id, const_str);
                    }
                    else {
                        const_str = std::get<std::string>(res2.constant->value); 
                        res_id = fn->add_int_c(res1.expr_id, const_str);
                    }
                }
                else {
                    res_id = fn->add_int(res1.expr_id, res2.expr_id);
                    fn->free_result(res1.expr_id);
                    fn->free_result(res2.expr_id);
                }

                expr_result res{C_INT, res_id, false, nullptr, nullptr};
                res_stack.push(res);

                opt_release_stack();
            }
            else {
                auto child = std::move(expr->children.front());
                expr->children.pop_front();
                op_stack.push(std::move(expr));
                expr = std::move(child);
            }
        }
        else {
            CRITICAL_ASSERT_NOW("Only binary '+' operator supported in expressions right now");
        }
    }


    return pop_res_stack();
}

static void eval_ret_stmt(std::unique_ptr<ast> cur_stmt_list, Ifunc_translation* fn, bool is_branch = false) {
    if(!cur_stmt_list->children.size()) {
        sim_log_error("return should be followed by expression!(Other case not supported right now)");
    }

    auto res = eval_expr(std::move(cur_stmt_list->children[0]), fn);

    if(is_branch) {
        fn->branch_return(res.expr_id);
    }
    else {
        fn->fn_return(res.expr_id);
    }

    fn->free_result(res.expr_id);
}

static void eval_stmt_list(std::unique_ptr<ast> cur_stmt_list, Ifunc_translation* fn) {

    std::stack<std::unique_ptr<ast>> stmt_stack;

    while(!stmt_stack.empty() || cur_stmt_list->children.size()) {
        if(!cur_stmt_list->children.size()) {
            cur_stmt_list = std::move(stmt_stack.top());
            stmt_stack.pop();
            auto parent = current_scope->fetch_parent_scope();
            delete current_scope;
            current_scope = parent;
            continue;
        }

        auto stmt = std::move(cur_stmt_list->children.front());
        cur_stmt_list->children.pop_front();

        if(stmt->is_stmt_list()) {
            stmt_stack.push(std::move(cur_stmt_list));
            cur_stmt_list = std::move(stmt);
            current_scope = new scope(current_scope);
        }
        else if(stmt->is_ret_stmt()) {
            eval_ret_stmt(std::move(stmt), fn);
        }
        else if(stmt->is_null_stmt())
            continue;
        else
            CRITICAL_ASSERT_NOW("Invalid statement encountered during evaluation");
    } 

}



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

            current_scope->add_variable(id, name, _type, val, true);
        } 
        else {
            int id = tu->declare_global_variable(name, _type);

            current_scope->add_variable(id, name, _type, true);
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

    auto fn_intf = tu->add_function(fn_name);
    current_scope->add_function_definition(fn_name, fn_intf);

    current_scope = new scope(current_scope);
    eval_stmt_list(std::move(fn_def->children[3]), fn_intf);
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