#include "core/ast.h"
#include "common/diag.h"

bool evaluator(std::string_view exp, diag* diag_inst = nullptr, size_t dir_start_idx = 0); 
bool eval(); 

extern std::unique_ptr<ast> g_ast;
extern diag* g_diag_inst;
extern size_t g_dir_start_idx;

static inline void print_error() {
    if(g_diag_inst)
        g_diag_inst->print_error(g_dir_start_idx);
}

