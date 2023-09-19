#include <tuple>
#include <optional>
#include <stack>
#include "compiler/ast.h"
#include "compiler/scope.h"
#include "compiler/ast-ops.h"
#include "compiler/compile.h"
#include "compiler/utils.h"
#include "compiler/eval.h"
#include "debug-api.h"
scope* current_scope = nullptr;
bool code_gen::eval_only = false;

//Fn scope
static void create_new_scope(Ifunc_translation* fn_intf) {
    auto new_scope = new scope(current_scope, fn_intf);
    current_scope = new_scope;
}

//Block scope
static void create_new_scope() {
    auto new_scope = new scope(current_scope);
    current_scope = new_scope;
}

static void revert_to_old_scope() {
    auto parent = current_scope->fetch_parent_scope();
    delete current_scope;

    current_scope = parent;
}

static void eval_expr_stmt(std::unique_ptr<ast> cur_stmt, Ifunc_translation* fn) {
    auto res = eval_expr(fetch_child(cur_stmt), fn, current_scope).eval();
    res.free(fn);
}

static void eval_ret_stmt(std::unique_ptr<ast> ret_stmt, Ifunc_translation* fn, const var_info& var, bool is_branch = false) {
    auto ret_type = var.type.resolve_type();
    if(!ret_stmt->children.size()) {
        if(!ret_type.is_void()) {
            sim_log_error("Function with non void return type, returning void expression");
        }
        return;
    }

    auto res = eval_expr(std::move(ret_stmt->children[0]), fn, current_scope).eval();
    if(ret_type.is_void() && !res.type.is_void()) {
        sim_log_error("Function with void return type, returning an expression");
    }

    if(!res.type.is_type_convertible(ret_type)) {
        sim_log_error("Expr type not compatible with return type of function");
    }

    res.convert_type(expr_result(ret_type), fn);
    if(is_branch) {
        if(res.is_constant)
            code_gen::call_code_gen(fn, &Ifunc_translation::branch_return, res.constant);
        else
            code_gen::call_code_gen(fn, &Ifunc_translation::branch_return, res.expr_id);
    }
    else {
        if(res.is_constant)
            code_gen::call_code_gen(fn, &Ifunc_translation::fn_return, res.constant);
        else
            code_gen::call_code_gen(fn, &Ifunc_translation::fn_return, res.expr_id);
    }
}

using base_type_res = std::tuple<c_type, bool, cv_info>;
static base_type_res forge_base_type(std::unique_ptr<ast_base_type> base_type) {
    c_type phy_type = C_INT;

    if(!base_type->base_type.type_spec) {
        phy_type = C_INT;
    }
    else {
        phy_type = base_type->base_type.fetch_type_spec();
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

    return std::make_tuple(phy_type, is_signed, cv_info{is_const, is_vol});    
}

static type_spec forge_type(const base_type_res& base_res, const decltype(type_spec::mod_list)& mod_list, const decl_spec& stor_spec) {
    return {std::get<0>(base_res), std::get<1>(base_res), std::get<2>(base_res), stor_spec.is_stor_register(), mod_list};
}

static std::vector<std::string_view> eval_decl_list(std::unique_ptr<ast> decl_list, bool is_fn_def = false) {

    struct context {
        std::unique_ptr<ast> decl_list;
        decltype(type_spec::mod_list) mod_list;
        base_type_res base_type;
        decl_spec stor_spec;
        context(std::unique_ptr<ast> decl, const decltype(type_spec::mod_list)& mod, 
        const decl_spec& stor, const base_type_res& base) :
        decl_list(std::move(decl)), mod_list(mod), stor_spec(stor), base_type(base)
        {}

        context(context&& obj) {
            decl_list = std::move(obj.decl_list);
            mod_list = obj.mod_list;
            stor_spec = obj.stor_spec;
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
    std::vector<std::string_view> fn_args;

    while(!decl_list->children.empty() || !iter_stack.empty()) {    
        //Retrieve the saved context and resume from there
        if(!decl_list->children.size()) {
            auto node = fetch_stack_node(iter_stack);
            decl_list = std::move(node.decl_list);
            mod_list = node.mod_list;
            stor_spec = node.stor_spec;
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
        std::unique_ptr<ast> init_expr;
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

                    //Save the decl_list, mod_list, stor_spec and base_type
                    context tmp(std::move(decl_list), mod_list, stor_spec, forged_base);
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
            else if(child->is_expr()) {
                //This declaration has an initializer
                init_expr = std::move(child);
            }

        }

        if(skip_to_new_decl) {
            skip_to_new_decl = false;
            continue;
        }

        type_spec res{};
        res = forge_type(forged_base, mod_list, stor_spec);
        if (res.base_type == C_VOID) {
            if (!res.mod_list.size()) {
                sim_log_error("void base type cannot be used with unmodified type");
            }
            else if (res.mod_list.back().is_array_mod()) {
                sim_log_error("array of void is not allowed");
            }
        }
        auto decl_view = pointer_cast<ast_decl>(decl)->ident;
        std::string_view name;
        if(decl_view) {
            name = std::get<std::string>(decl_view->value);
        } 
        if(iter_stack.empty()) {    
            //Push the function name along with it's args for ease of processing
            if(is_fn_def) {
                fn_args.push_back(name);
            }
            bool is_global = current_scope->is_global_scope();
            current_scope->add_variable(name, res, stor_spec, std::move(init_expr), is_global);
        }
        else {
            //Only top level fn arguments should get pushed to list
            if(is_fn_def && iter_stack.size() == 1) {
                fn_args.push_back(name);
            }
            saved_res = res; 
        } 
        mod_list.clear();
    }

    return fn_args;
}

static std::unique_ptr<ast> eval_while_stmt(std::unique_ptr<ast> stmt, Ifunc_translation* fn, 
std::stack<std::unique_ptr<ast>>& stmt_stack, std::stack<std::tuple<int, int, bool>>& if_stack,
std::unique_ptr<ast> cur_stmt_list, std::stack<std::tuple<bool, bool, bool>>& branch_status) {

    //stmt list has been evaluated
    if(!stmt->children.size()) {
        auto [start_id, end_id, _] = fetch_stack_node(if_stack);
        code_gen::call_code_gen(fn, &Ifunc_translation::branch, start_id);
        code_gen::call_code_gen(fn, &Ifunc_translation::insert_label, end_id);
        return cur_stmt_list;
    }

    int start_id = code_gen::call_code_gen(fn, &Ifunc_translation::create_label);
    int end_id = code_gen::call_code_gen(fn, &Ifunc_translation::create_label); 
    code_gen::call_code_gen(fn, &Ifunc_translation::insert_label, start_id);
    auto expr = fetch_child(stmt);
    auto res = eval_expr(std::move(expr), fn, current_scope).eval();

    if(res.is_constant) {
        if(res.constant == "0") {
            code_gen::call_code_gen(fn, &Ifunc_translation::branch, end_id);
        }
    }
    else {
        code_gen::call_code_gen(fn, &Ifunc_translation::branch_if_z, res.expr_id, end_id);
    }
    res.free(fn);

    if_stack.push(std::make_tuple(start_id, end_id, false));
    auto stmt_list = fetch_child(stmt);
    cur_stmt_list->attach_node(std::move(stmt)); //Reattach node

    stmt_stack.push(std::move(cur_stmt_list));
    branch_status.push(std::make_tuple(true, std::get<1>(branch_status.top()), std::get<2>(branch_status.top())));

    create_new_scope();
    return stmt_list;
}
static std::unique_ptr<ast> eval_if_stmt(std::unique_ptr<ast> stmt, Ifunc_translation* fn, 
std::stack<std::unique_ptr<ast>>& stmt_stack, std::stack<std::tuple<int, int, bool>>& if_stack,
std::unique_ptr<ast> cur_stmt_list, std::stack<std::tuple<bool, bool, bool>>& branch_status) {
    
    auto prepare_stmt_list_jump = [&] (std::unique_ptr<ast> node) {
        auto stmt_list = fetch_child(node);
        if (node->is_if())
            cur_stmt_list->attach_node(std::move(node)); //Reattach if stmt back to stmt list
        else
            cur_stmt_list->attach_node(std::move(stmt));
        stmt_stack.push(std::move(cur_stmt_list));

        //It is an 'if' branch and inherits eval_status from it's parent
        branch_status.push(std::make_tuple(true, std::get<1>(branch_status.top()), std::get<2>(branch_status.top())));
        create_new_scope();
        return stmt_list;
    };

    auto indirect_branch = [&] (const expr_result& res, int label_id) {
        if(res.is_constant) {
            if(res.constant == "0") {
                code_gen::call_code_gen(fn, &Ifunc_translation::branch, label_id);
            }
        }
        else {
            code_gen::call_code_gen(fn, &Ifunc_translation::branch_if_z, res.expr_id, label_id);
        }
    };

    auto handle_branch_jump = [&] (std::unique_ptr<ast> node) {
        auto expr = fetch_child(node);
        auto res = eval_expr(std::move(expr), fn, current_scope).eval();
        bool is_end = false;
        if((node->is_if() && node->children.size() == 1) || (!node->is_if() && !stmt->children.size())) {
            is_end = true;
        }
        
        int next_id = 0;
        //If we reached last branch, then jump to end
        if(!node->is_if() && is_end) {
            indirect_branch(res, std::get<1>(if_stack.top()));
        }
        else {
            next_id = code_gen::call_code_gen(fn, &Ifunc_translation::create_label);
            indirect_branch(res, next_id);
        }
        res.free(fn);

        if(node->is_if()) {
            int end_id = is_end ? next_id : code_gen::call_code_gen(fn, &Ifunc_translation::create_label);
            if_stack.push(std::make_tuple(next_id, end_id, false));
        }
        else {
            auto info = fetch_stack_node(if_stack);
            std::get<0>(info) = next_id;
            if_stack.push(info);
        }

        return prepare_stmt_list_jump(std::move(node));
    };
    
    if (!stmt->children.size()) {
        auto [_, end_id, __] = fetch_stack_node(if_stack);
        code_gen::call_code_gen(fn, &Ifunc_translation::insert_label, end_id);
        return cur_stmt_list;
    }

    if(stmt->children[0]->is_expr()) {
        return handle_branch_jump(std::move(stmt));
    }
    
    //Jump to end for first if branch
    bool& evaluated = std::get<2>(if_stack.top());
    if(!evaluated) {
        evaluated = true;
    }
   
    //Handle else if and else blocks
    auto child = fetch_child(stmt);
    code_gen::call_code_gen(fn, &Ifunc_translation::branch, std::get<1>(if_stack.top()));
    code_gen::call_code_gen(fn, &Ifunc_translation::insert_label, std::get<0>(if_stack.top()));
    if(child->is_else_if()) {
        return handle_branch_jump(std::move(child));
    }
    else {
        return prepare_stmt_list_jump(std::move(child));
    }
}

static void eval_stmt_list(std::unique_ptr<ast> cur_stmt_list, Ifunc_translation* fn, std::string_view fn_name) {
    std::stack<std::unique_ptr<ast>> stmt_stack;
    std::stack<std::tuple<int, int, bool>> if_stack;
    std::stack<std::tuple<bool, bool, bool>> branch_status; //is_if, is_eval_only, is_nested_in_if
    std::optional<var_info> fn_decl;
    code_gen::eval_only = false;
    bool ret_encountered = false;

    branch_status.push(std::make_tuple(false, false, false));
    while(!stmt_stack.empty() || cur_stmt_list->children.size()) {
        if(!cur_stmt_list->children.size()) {
            cur_stmt_list = fetch_stack_node(stmt_stack);
            revert_to_old_scope();
            auto [is_if, is_eval_only, _] = fetch_stack_node(branch_status);
            if(!is_if) {
                std::get<1>(branch_status.top()) = is_eval_only;
            }
            code_gen::eval_only = std::get<1>(branch_status.top());
            continue;
        }
        auto stmt = fetch_child(cur_stmt_list);

        if(stmt->is_stmt_list()) {
            stmt_stack.push(std::move(cur_stmt_list));
            
            //Eval and branch status doesn't change when we go to inner branch
            branch_status.push(std::make_tuple(false, code_gen::eval_only, 
            std::get<0>(branch_status.top()) || std::get<2>(branch_status.top()))); 
            cur_stmt_list = std::move(stmt);
            create_new_scope();
        }
        else if(stmt->is_decl_stmt()) {
            eval_decl_list(fetch_child(stmt));
        }
        else if(stmt->is_expr_stmt()) {
            eval_expr_stmt(std::move(stmt), fn);
        }
        else if(stmt->is_while()) {
            cur_stmt_list = eval_while_stmt(std::move(stmt), fn, stmt_stack, if_stack, std::move(cur_stmt_list), branch_status);
        }
        else if(stmt->is_if()) {
            cur_stmt_list = eval_if_stmt(std::move(stmt), fn, stmt_stack, if_stack, std::move(cur_stmt_list), branch_status);
        }
        else if(stmt->is_ret_stmt()) {
            if(!fn_decl)
                fn_decl = current_scope->fetch_var_info(fn_name);
            eval_ret_stmt(std::move(stmt), fn, *fn_decl, std::get<0>(branch_status.top()) || std::get<2>(branch_status.top()));
            code_gen::eval_only = true;
            std::get<1>(branch_status.top()) = true;
            ret_encountered = true;
        }
        else if(stmt->is_null_stmt())
            continue;
        else
            CRITICAL_ASSERT_NOW("Invalid statement encountered during evaluation");
    }

    if(!ret_encountered) {
        auto& fn_info = current_scope->fetch_var_info(fn_name);
        if(!fn_info.type.resolve_type().is_void()) {
            sim_log_error("Function with non void return type is not returning any expression");
        }
    } 

}

void eval_fn_def(std::unique_ptr<ast> fn_def) {
    auto fn_args = eval_decl_list(fetch_child(fn_def), true);
    auto [fn_intf, fn_scope] = current_scope->add_function_definition(fn_args.back(), std::vector<std::string_view>(fn_args.begin(), fn_args.begin() + fn_args.size() - 1));

    current_scope = fn_scope;
    eval_stmt_list(fetch_child(fn_def), fn_intf, fn_args.back());
    revert_to_old_scope();
}

void eval(std::unique_ptr<ast> prog) {
    sim_log_debug("Starting evaluator...");
    CRITICAL_ASSERT(prog->is_prog(), "eval() called with non program node");

    auto tu = std::shared_ptr<Itranslation>(create_translation_unit());
    current_scope = new scope(nullptr, tu);

    for(auto& child: prog->children) {
        if(child->is_decl_list()) {
            eval_decl_list(std::move(child));
        }
        else if(child->is_fn_def()) {
            eval_fn_def(std::move(child));
        }
        else {
            CRITICAL_ASSERT_NOW("Invalid ast node found as child of program node");
        }
    }

    tu->generate_code();
    asm_code = tu->fetch_code();
}