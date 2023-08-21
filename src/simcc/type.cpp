#include "compiler/type.h"
#include "debug-api.h"

c_type decl_spec::fetch_type_spec() const {
    switch(std::get<keyword_type>(type_spec->value)) {
        case TYPE_INT: return C_INT;
        case TYPE_CHAR: return C_CHAR;
        case TYPE_VOID: return C_VOID;
        case TYPE_LONGLONG: return C_LONGLONG;
        case TYPE_LONG: return C_LONG;
        case TYPE_SHORT: return C_SHORT;
        default: CRITICAL_ASSERT_NOW("Invalid type token mentioned in decl_specifier");
    }
}

bool decl_spec::is_const() const {
    return const_qual->is_keyword_const();
}

bool decl_spec::is_volatile() const {
    return vol_qual->is_keyword_volatile();
}

bool decl_spec::is_stor_auto() const {
    return storage_spec->is_keyword_auto();
}

bool decl_spec::is_stor_extern() const {
    return storage_spec->is_keyword_extern();
}

bool decl_spec::is_stor_register() const {
    return storage_spec->is_keyword_register();
}

bool decl_spec::is_stor_static() const {
    return storage_spec->is_keyword_static();
}
