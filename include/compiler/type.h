#pragma once

#include "compiler/token.h"
#include "lib/code-gen.h"

struct decl_spec {
    const token* storage_spec;
    const token* type_spec;
    const token* const_qual, *vol_qual;
    const token* sign_qual;

    bool is_stor_static() const;
    bool is_stor_extern() const;
    bool is_stor_auto() const;
    bool is_stor_register() const;

    c_type fetch_type_spec() const;
    bool is_signed() const;

    bool is_const() const;
    bool is_volatile() const;
};