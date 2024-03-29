#pragma once

#include "core/ast.h"
#include "common/diag.h"

void lex(const std::vector<char>& input);
std::unique_ptr<ast> parse();
void eval(std::unique_ptr<ast>);

extern std::string asm_code;
