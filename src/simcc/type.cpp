#include "compiler/type.h"
#include "debug-api.h"

c_type decl_spec::fetch_type_spec() const {
    switch(std::get<keyword_type>(type_spec->value)) {
        case TYPE_INT: return C_INT;
        case TYPE_CHAR: return C_CHAR;
        case TYPE_LONGLONG: return C_LONGLONG;
        case TYPE_LONG: return C_LONG;
        case TYPE_SHORT: return C_SHORT;
        default: CRITICAL_ASSERT_NOW("Invalid type token mentioned in decl_specifier");
    }
}

bool decl_spec::is_const() const {
    return const_qual && const_qual->is_keyword_const();
}

bool decl_spec::is_volatile() const {
    return vol_qual && vol_qual->is_keyword_volatile();
}

bool decl_spec::is_stor_auto() const {
    return storage_spec && storage_spec->is_keyword_auto();
}

bool decl_spec::is_stor_extern() const {
    return storage_spec && storage_spec->is_keyword_extern();
}

bool decl_spec::is_stor_register() const {
    return storage_spec && storage_spec->is_keyword_register();
}

bool decl_spec::is_stor_static() const {
    return storage_spec && storage_spec->is_keyword_static();
}

bool decl_spec::is_signed() const {
    return sign_qual && sign_qual->is_keyword_signed();
}


type_spec type_spec::resolve_type() const {
    type_spec tmp = *this;

    if(tmp.mod_list.size()) {
        if(is_pointer_type()) {
            tmp.mod_list[0].ptr_list.pop_front();
            if(!tmp.mod_list[0].ptr_list.size())
                tmp.mod_list.pop_front();
        }
        else if(is_array_type()) {
            tmp.mod_list[0].array_spec.pop_front();
            if(!tmp.mod_list[0].array_spec.size())
                tmp.mod_list.pop_front();
        }
        else if(is_function_type()) {
            tmp.mod_list.pop_front();
        }
    }

    return tmp;
}

bool cv_info::operator == (const cv_info& info) const {
    return is_const == info.is_const && is_volatile == info.is_volatile;
}

bool type_spec::is_pointer_type() const {
    return mod_list.size() && mod_list[0].ptr_list.size();
}

bool type_spec::is_array_type() const {
    return mod_list.size() && mod_list[0].array_spec.size();
}

bool type_spec::is_function_type() const {
    return mod_list.size() && (mod_list[0].fn_spec.size() || (!is_array_type() && !is_pointer_type()));
}

bool type_spec::operator == (const type_spec& type) const {
    if(is_void) {
        if(!type.is_void)
            return false;
    }

    if(!(base_type == type.base_type && is_signed == type.is_signed && 
    cv.is_const == type.cv.is_const && cv.is_volatile == type.cv.is_volatile))
        return false;

    if(mod_list.size() != type.mod_list.size())
        return false;

    for(size_t i = 0; i < mod_list.size(); i++) {
        if(mod_list[i].ptr_list.size()) {
            for(size_t j = 0; j < mod_list[i].ptr_list.size(); j++) {
                if(!(mod_list[i].ptr_list[j] == type.mod_list[i].ptr_list[j]))
                    return false;
            }
        }
        else if(mod_list[i].array_spec.size()) {
            for(size_t j = 0; j < mod_list[i].array_spec.size(); j++) {
                if(mod_list[i].array_spec[j] != type.mod_list[i].array_spec[j])
                    return false;
            }
        }
        else {
            for(size_t j = 0; j < mod_list[i].fn_spec.size(); j++) {
                if(!(mod_list[i].fn_spec[j] == type.mod_list[i].fn_spec[j]))
                    return false;
            }
        }
    }

    return true;
}

bool modifier::is_function_mod() const {
    if(fn_spec.size() || (!is_array_mod() && !is_pointer_mod()))
        return true;
    
    return false;
}

bool modifier::is_array_mod() const {
    return array_spec.size();
}

bool modifier::is_pointer_mod() const {
    return ptr_list.size();
}