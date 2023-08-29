#include <optional>
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
    bool is_global;
    int global_id;
};

/*
static expr_result eval_expr(std::unique_ptr<ast> expr, Ifunc_translation* fn) {
    std::stack<expr_result> res_stack;
    std::stack<std::unique_ptr<ast>> op_stack;


    enum eval_context {
        NORMAL,
        ASSIGNABLE
    };

    std::stack<eval_context> context_stack;

    auto opt_release_stack = [&] {
        if (op_stack.empty()) {
            expr.reset();
        }
        else {
            expr = std::move(op_stack.top());
            op_stack.pop();
        }
    };

    auto pop_res_stack = [&] {

        if (res_stack.empty())
            return expr_result{};

        auto _val = res_stack.top();
        res_stack.pop();
        return _val;
    };

    auto is_assignable_context = [&] {
        return !context_stack.empty() && context_stack.top() == ASSIGNABLE;
    };

    auto pop_context_stack = [&] {
        if (!context_stack.empty())
            context_stack.pop();
    };

    sim_log_debug("Evaluating expression");
    while (!op_stack.empty() || expr) {
        auto _expr = cast_to_ast_expr(expr);
#ifdef SIMDEBUG
        _expr->tok->print();
#endif
        if (_expr->is_con()) {
            CRITICAL_ASSERT(_expr->tok->is_integer_constant(), "Only integer constants supported now");
            expr_result tmp{};
            tmp.eval_type = C_INT;
            tmp.is_lvalue = false;
            tmp.constant = _expr->tok;

            res_stack.push(tmp);
            opt_release_stack();
        }
        else if (_expr->is_var()) {
            auto& var = current_scope->fetch_var_info(std::get<std::string>(_expr->tok->value));
            //CRITICAL_ASSERT(var.var.type == C_INT, "Only integer type variables supported now");

            expr_result tmp{};
            tmp.eval_type = var.var.type;
            tmp.expr_id = var.var_id;
            tmp.is_lvalue = true;
            tmp.variable = _expr->tok;

            if (var.is_global) {
                if (!is_assignable_context())
                    tmp.expr_id = fn->fetch_global_var(var.var_id);
                tmp.global_id = var.var_id;
                tmp.is_global = true;
            }

            res_stack.push(tmp);
            opt_release_stack();
        }
        else if (_expr->is_operator()) {
            auto _op = cast_to_ast_op(expr);
            CRITICAL_ASSERT(!_op->is_unary, "Only binary '+' and '=' operators supported in expression right now");


            if (!expr->children.size()) {
                //Right now we assume it to be binary operator
                auto res1 = pop_res_stack(); //right child
                auto res2 = pop_res_stack(); //left child

                int res_id = 0;

                context_stack.pop(); //Pop both context values
                context_stack.pop();

                CRITICAL_ASSERT(!(res1.constant && res2.constant), "Both operands being constants is not supported right now");

                int (Ifunc_translation:: * fn_bin_c)(int, std::string_view) = &Ifunc_translation::add;
                int (Ifunc_translation:: * fn_bin_norm)(int, int) = &Ifunc_translation::add;
                auto op_type = std::get<operator_type>(_op->tok->value);
                switch (op_type) {
                case PLUS: fn_bin_c = &Ifunc_translation::add; fn_bin_norm = &Ifunc_translation::add; break;
                case MINUS: fn_bin_c = &Ifunc_translation::sub; fn_bin_norm = &Ifunc_translation::sub; break;
                case EQUAL: break;
                default: CRITICAL_ASSERT_NOW("Only '+', '-' and '=' operators supported right now");
                }

                if (op_type == EQUAL) {
                    int (Ifunc_translation:: * fn_bin_c)(int, std::string_view);
                    int (Ifunc_translation:: * fn_bin_norm)(int, int);

                    if (res2.is_global) {
                        fn_bin_c = &Ifunc_translation::assign_global_var;
                        fn_bin_norm = &Ifunc_translation::assign_global_var;
                    }
                    else {
                        fn_bin_c = &Ifunc_translation::assign_var;
                        fn_bin_norm = &Ifunc_translation::assign_var;
                    }

                    if (!res2.is_lvalue) {
                        sim_log_error("LHS of '=' should be lvalue expression");
                    }
                    int lhs_id = res2.expr_id;
                    if (res2.is_global)
                        lhs_id = res2.global_id;

                    if (res1.constant) {
                        std::string_view const_str = std::get<std::string>(res1.constant->value);
                        (fn->*fn_bin_c)(lhs_id, const_str);
                    }
                    else {
                        (fn->*fn_bin_norm)(lhs_id, res1.expr_id);
                        fn->free_result(res1.expr_id);
                    }

                    if (is_assignable_context())
                        res_stack.push(res2);
                    else if (!context_stack.empty()) {
                        int local_id = fn->fetch_global_var(res2.global_id);
                        expr_result tmp{};
                        tmp.eval_type = res1.eval_type;
                        tmp.global_id = res2.global_id;
                        tmp.expr_id = local_id;
                        tmp.is_lvalue = false;
                        tmp.is_global = true;

                        res_stack.push(tmp);
                    }
                    opt_release_stack();
                    continue;
                }

                if (res1.constant || res2.constant) {
                    std::string_view const_str;
                    if (res1.constant) {
                        const_str = std::get<std::string>(res1.constant->value);
                        res_id = (fn->*fn_bin_c)(res2.expr_id, const_str);
                    }
                    else {
                        const_str = std::get<std::string>(res2.constant->value);
                        if (op_type == MINUS) {
                            res_id = fn->sub(const_str, res1.expr_id);
                        }
                        else
                            res_id = (fn->*fn_bin_c)(res1.expr_id, const_str);
                    }
                }
                else {
                    res_id = (fn->*fn_bin_norm)(res1.expr_id, res2.expr_id);
                }

                expr_result res{};
                res.eval_type = res1.eval_type;
                res.expr_id = res_id;
                res.is_lvalue = false;

                res_stack.push(res);
                opt_release_stack();
            }
            else {
                auto child = std::move(expr->children.front());
                expr->children.pop_front();
                if (_op->tok->is_operator_eq()) {
                    if (expr->children.size())
                        context_stack.push(ASSIGNABLE); //Push ASSIGNABLE context for left child
                    else
                        context_stack.push(NORMAL); //Push normal for right child
                }
                else {
                    context_stack.push(NORMAL);
                }
                op_stack.push(std::move(expr));
                expr = std::move(child);
            }
        }
        else {
            CRITICAL_ASSERT_NOW("Function call operation not supported right now");
        }
    }

    return pop_res_stack();
}
*/

/*
static void eval_expr_stmt(std::unique_ptr<ast> cur_stmt, Ifunc_translation* fn) {

    auto res = eval_expr(std::move(cur_stmt->children[0]), fn);

    if(res.expr_id)
        fn->free_result(res.expr_id);

}
*/
/*
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
*/
/*
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
        else if(stmt->is_expr_stmt()) {
            eval_expr_stmt(std::move(stmt), fn);
        }
        else if(stmt->is_null_stmt())
            continue;
        else
            CRITICAL_ASSERT_NOW("Invalid statement encountered during evaluation");
    } 

}
*/

/*
static void eval_fn_def(std::unique_ptr<ast> fn_def, std::shared_ptr<Itranslation> tu) {
    const auto& fn_name = std::get<std::string>(cast_to_ast_token(fn_def->children[1])->tok->value); 
    setup_fn_decl(std::move(fn_def->children[0]), std::move(fn_def->children[1]), std::move(fn_def->children[2]), tu);

    auto fn_intf = tu->add_function(fn_name);
    current_scope->add_function_definition(fn_name, fn_intf);

    current_scope = new scope(current_scope);
    eval_stmt_list(std::move(fn_def->children[3]), fn_intf);
    auto parent = current_scope->fetch_parent_scope();
    delete current_scope;
    current_scope = parent;
}
*/

static std::unique_ptr<ast> fetch_child(std::unique_ptr<ast>& parent) {
    auto child = std::move(parent->children.front());
    parent->children.pop_front();

    return child;
}

template<typename Ast>
static Ast fetch_stack_node(std::stack<Ast>& iter_stack) {
    auto child = std::move(iter_stack.top());
    iter_stack.pop();

    return child;
}

template<typename Ast>
static std::unique_ptr<Ast> fetch_child_and_cast(std::unique_ptr<ast>& parent) {
    return std::unique_ptr<Ast>(static_cast<Ast*>(fetch_child(parent).release()));
}

template<typename Ast, typename Predicate>
static void list_consume(std::unique_ptr<ast> child, Predicate&& lambda) {
    while(!child->children.empty()) {
        auto ptr_child = fetch_child_and_cast<Ast>(child);
        lambda(std::move(ptr_child));
    }
}

template<typename Ast>
static const Ast* pointer_cast(const std::unique_ptr<ast>& ptr) {
    return static_cast<Ast*>(ptr.get());
}

static struct base_type_res {
    c_type phy_type;
    bool is_void;
    bool is_signed;
    cv_info cv;
} forge_base_type(std::unique_ptr<ast_base_type> base_type) {
    c_type phy_type = C_INT;
    bool is_void = false;

    if(!base_type->base_type.type_spec) {
        phy_type = C_INT;
    }
    else {
        if(std::get<keyword_type>(base_type->base_type.type_spec->value) == TYPE_VOID) {
            is_void = true;
        }
        else {
            phy_type = base_type->base_type.fetch_type_spec();
        }
    }
    bool has_sign = false, is_signed = true;
    if(base_type->base_type.sign_qual) {
        has_sign = true;
        if(base_type->base_type.sign_qual->is_keyword_unsigned())
            is_signed = false;
    }

    bool is_const = false, is_vol = false;
    if(base_type->base_type.const_qual)
        is_const = true;
    if(base_type->base_type.vol_qual)
        is_vol = true;

    return {phy_type, is_void, is_signed, cv_info{is_const, is_vol}};    
}

static type_spec forge_type(const base_type_res& base_res, const decltype(type_spec::mod_list)& mod_list) {
    return {base_res.phy_type, base_res.is_void, base_res.is_signed, base_res.cv, mod_list};
}

static void eval_decl_list(std::unique_ptr<ast> decl_list, std::shared_ptr<Itranslation> tu) {

    struct context {
        std::unique_ptr<ast> decl_list;
        decltype(type_spec::mod_list) mod_list;
        base_type_res base_type;

        context(std::unique_ptr<ast> decl, const decltype(type_spec::mod_list)& mod, const base_type_res& base) :
        decl_list(std::move(decl)), mod_list(mod), base_type(base)
        {}

        context(context&& obj) {
            decl_list = std::move(obj.decl_list);
            mod_list = obj.mod_list;
            base_type = obj.base_type;
        }
    };

    std::stack<context> iter_stack;
    std::optional<type_spec> saved_res;

    auto base_type = fetch_child_and_cast<ast_base_type>(decl_list);
    auto stor_spec = base_type->base_type;
    auto forged_base = forge_base_type(std::move(base_type));
    bool base_type_init = true;
    bool skip_to_new_decl = false;
    bool array = false, function = false;
    decltype(type_spec::mod_list) mod_list;

    while(!decl_list->children.empty() || !iter_stack.empty()) {    
        //Retrieve the saved context and resume from there
        if(!decl_list->children.size()) {
            auto node = fetch_stack_node(iter_stack);
            decl_list = std::move(node.decl_list);
            mod_list = node.mod_list;
            forged_base = node.base_type;
            
            base_type_init = true;
            continue;
        }

        if(!base_type_init) {
            base_type = fetch_child_and_cast<ast_base_type>(decl_list);
            stor_spec = base_type->base_type;
            forged_base = forge_base_type(std::move(base_type));
            base_type_init = true;
        }
        array = function = false;
        auto decl = fetch_child(decl_list);
        while(!decl->children.empty()) {
            auto child = fetch_child(decl);

            if(child->is_pointer_list()) {
                modifier ptr_mod;
                list_consume<ast_ptr_spec>(std::move(child), [&] (std::unique_ptr<ast_ptr_spec> ptr_child) {
                    ptr_mod.ptr_list.push_back(cv_info{static_cast<bool>(ptr_child->const_qual), static_cast<bool>(ptr_child->vol_qual)});
                });
                mod_list.push_back(ptr_mod);

                array = function = false;
            }
            else if(child->is_array_specifier_list()) {

                if(function) {
                    sim_log_error("Function cannot return array type");
                }

                array = true;
                function = false;

                modifier array_spec;
                list_consume<ast_array_spec>(std::move(child), [&] (std::unique_ptr<ast_array_spec> array_child) {
                    array_spec.array_spec.push_back(std::get<std::string>(array_child->constant->value));
                });
                mod_list.push_back(array_spec);
            }
            else if(child->is_param_list()) {
                if(array) {
                    sim_log_error("Modified type of array of functions not allowed");
                }
                if(function) {
                    sim_log_error("Function cannot return function type");
                }

                if(saved_res) {
                    if(mod_list.size() && mod_list.back().is_function_mod())
                        mod_list.back().fn_spec.push_back(saved_res.value());
                    else {
                        modifier tmp{};
                        tmp.fn_spec.push_back(saved_res.value());
                        mod_list.push_back(tmp);
                    }
                    saved_res.reset();
                }
                else {
                    //If no saved result and param list is empty, push empty fn_spec 
                    if(!child->children.size()) {
                        mod_list.push_back(modifier{});
                    }
                }

                if(child->children.size()) {
                    auto param = fetch_child(child);
                    
                    //Reattach param_list to decl and decl to decl_list before saving them(We need to resume from there)
                    decl->attach_node(std::move(child));
                    decl_list->attach_node(std::move(decl));

                    //Save the decl_list, mod_list and base_type
                    context tmp(std::move(decl_list), mod_list, forged_base);
                    iter_stack.push(std::move(tmp));
                    decl_list = std::move(param);
                    base_type_init = false;
                    skip_to_new_decl = true;
                    mod_list.clear();
                    break;
                }

                array = false;
                function = true;
            }

        }

        if(skip_to_new_decl) {
            skip_to_new_decl = false;
            continue;
        }

        type_spec res{};
        res = forge_type(forged_base, mod_list);
        if (res.is_void) {
            if (!res.mod_list.size()) {
                sim_log_error("void base type cannot be used with unmodified type");
            }
            else if (res.mod_list.back().is_array_mod()) {
                sim_log_error("array of void is not allowed");
            }
        }
        if(iter_stack.empty()) {     
            current_scope->add_variable(0, std::get<std::string>(pointer_cast<ast_decl>(decl)->ident->value),
        res, stor_spec);
        }
        else {
            saved_res = res; 
        } 

        mod_list.clear();

    }

}

void eval(std::unique_ptr<ast> prog) {
    sim_log_debug("Starting evaluator...");
    CRITICAL_ASSERT(prog->is_prog(), "eval() called with non program node");

    current_scope = new scope();
    auto tu = std::shared_ptr<Itranslation>(create_translation_unit());

    for(auto& child: prog->children) {
        if(child->is_decl_list()) {
            eval_decl_list(std::move(child), tu);
        }
        else {
            CRITICAL_ASSERT_NOW("Invalid ast node found as child of program node");
        }
    }

    //tu->generate_code();
    //asm_code = tu->fetch_code();
}