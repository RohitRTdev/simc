#pragma once
#include <stack>
#include "core/ast.h"
#include "debug-api.h"

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
    auto _ptr = dynamic_cast<Ast*>(fetch_child(parent).release()); 
    CRITICAL_ASSERT(_ptr, "fetch_child_and_cast called with invalid pointer");
    return std::unique_ptr<Ast>(_ptr);
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
    auto _ptr = dynamic_cast<Ast*>(ptr.get());
    CRITICAL_ASSERT(_ptr, "invalid pointer_cast");

    return _ptr;
}

struct code_gen {
    static bool eval_only;
    
    template<typename T, typename... Args> 
    static int call_code_gen(T* intf, int (T::*member)(Args...), const Args&... args) {
        if(!eval_only) {
            return (intf->*member)(args...);
        }

        return 0;
    }
    
    template<typename T, typename... Args> 
    static void call_code_gen(T* intf, void (T::*member)(Args...), const Args&... args) {
        if(!eval_only) {
            (intf->*member)(args...);
        }
    }

};
