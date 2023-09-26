#include "compiler/type.h"
#include "compiler/utils.h"
#include "debug-api.h"

void decl_spec::print_error() const {
    if(type_spec) {
        type_spec->print_error();
    }
    else if(storage_spec) {
        storage_spec->print_error();
    }
    else if(const_qual) {
        const_qual->print_error();
    }
    else if(vol_qual) {
        vol_qual->print_error();
    }
    else if(sign_qual) {
        sign_qual->print_error();
    }

}

c_type decl_spec::fetch_type_spec() const {
    switch(std::get<keyword_type>(type_spec->value)) {
        case TYPE_INT: return C_INT;
        case TYPE_CHAR: return C_CHAR;
        case TYPE_LONGLONG: return C_LONGLONG;
        case TYPE_LONG: return C_LONG;
        case TYPE_SHORT: return C_SHORT;
        case TYPE_VOID: return C_VOID;
        default: CRITICAL_ASSERT_NOW("Invalid type token mentioned in decl_specifier");
    }
}

bool decl_spec::is_stor_none() const {
    return !storage_spec;
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

type_spec type_spec::addr_type() const {
    type_spec tmp = *this;

    tmp.mod_list.push_front(modifier{cv_info{}});
    return tmp;
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

size_t type_spec::get_pointer_base_type_size() const {
    CRITICAL_ASSERT(is_pointer_type(), "get_pointer_base_type_size() called on non pointer type");

    return resolve_type().get_size();
}

size_t type_spec::get_size() const {
    size_t cur_size = 1;
    for(const auto& mod: mod_list) {
        if(mod.is_pointer_mod()) {
            return cur_size * base_type_size(C_PTR);
        }
        else if(mod.is_array_mod()) {
            for(const auto& val: mod.array_spec) {
                cur_size *= std::stoi(val); 
            }
        }
    }

    return cur_size * base_type_size(base_type);
} 
bool type_spec::is_incomplete_type() const {
    auto type = resolve_type();
    if(is_void() || is_function_type() || type.is_function_type())
        return true;
    else if(is_pointer_type() && type.is_void())
        return true;

    return false;
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

bool type_spec::is_integral() const {
    return !is_modified_type() && base_type != C_VOID;
}

bool type_spec::is_modifiable() const {
    if(is_void() || is_array_type() || is_function_type())
        return false;
    
    if(is_pointer_type()) {
        return !mod_list[0].ptr_list[0].is_const;
    }

    return !cv.is_const;
}

bool type_spec::is_convertible_to_pointer_type() {
    return is_array_type() || is_function_type();
}

void type_spec::convert_to_pointer_type() {
    CRITICAL_ASSERT(is_array_type() || is_function_type(), "convert_to_pointer_type() called on non array/function type");

    if(is_array_type()) {
        mod_list[0].array_spec.pop_front();
        if(mod_list[0].array_spec.size()) {
            mod_list.push_front(modifier{cv_info{}});
        }
        else {
            mod_list[0].ptr_list.push_back(cv_info{});
        }
    }
    else if(is_function_type()) {
        //Make it a pointer to function type
        mod_list.push_front(modifier{cv_info{}});
    }
}

bool type_spec::is_modified_type() const {
    return is_pointer_type() || is_array_type() || is_function_type();
}

bool type_spec::is_type_operable(const type_spec& type) const {
   if(is_void() || type.is_void() || is_modified_type() || type.is_modified_type()) {
        return false;
   }

   return true; 
}

bool type_spec::operator == (const type_spec& type) const {
    if(!(base_type == type.base_type && is_signed == type.is_signed && 
    cv.is_const == type.cv.is_const && cv.is_volatile == type.cv.is_volatile &&
    is_register == type.is_register))
        return false;

    if(mod_list.size() != type.mod_list.size())
        return false;

    for(size_t i = 0; i < mod_list.size(); i++) {
        if(mod_list[i].is_pointer_mod()) {
            if (!type.mod_list[i].is_pointer_mod() || (type.mod_list[i].ptr_list.size() != mod_list[i].ptr_list.size()))
                return false;
            for(size_t j = 0; j < mod_list[i].ptr_list.size(); j++) {
                if(!(mod_list[i].ptr_list[j] == type.mod_list[i].ptr_list[j]))
                    return false;
            }
        }
        else if(mod_list[i].is_array_mod()) {
            if (!type.mod_list[i].is_array_mod() || (type.mod_list[i].array_spec.size() != mod_list[i].array_spec.size()))
                return false;
            for(size_t j = 0; j < mod_list[i].array_spec.size(); j++) {
                if(mod_list[i].array_spec[j] != type.mod_list[i].array_spec[j])
                    return false;
            }
        }
        else {
            if(!type.mod_list[i].is_function_mod() || (mod_list[i].fn_spec.size() != type.mod_list[i].fn_spec.size()))
                return false;
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

bool type_spec::is_void() const {
    return !is_modified_type() && base_type == C_VOID;
}

bool type_spec::is_const() const {
    return !is_function_type() && ((is_pointer_type() && mod_list[0].ptr_list[0].is_const)
    || ((!is_modified_type() || is_array_type()) && cv.is_const));
} 

bool type_spec::is_type_convertible(type_spec& type) {
    if(type.is_array_type() || type.is_function_type()) {
        type.convert_to_pointer_type();
    }
    if(is_pointer_type() || !is_modified_type())
        return true;
    
    return false;
}

bool type_spec::is_pointer_to_function() const {
    return is_pointer_type() && resolve_type().is_function_type();
}

std::pair<c_type, bool> type_spec::get_simple_type() const {
    c_type sim_type = base_type;
    bool sim_sign = is_signed;
    if(is_modified_type()) {
        sim_type = C_PTR;
        sim_sign = false;
    }

    return std::make_pair(sim_type, sim_sign);
}

int type_spec::convert_type(const type_spec& dest_type, int src_id, type_spec& src_type, Ifunc_translation* fn_intf, bool do_phy_conv) {
    CRITICAL_ASSERT(src_type.is_pointer_type() || !src_type.is_modified_type(), 
    "Array/function type passed as source type during call to convert_type()");

    auto [base_type, is_signed] = dest_type.get_simple_type();

    sim_log_debug("Converting type:{} with sign:{} to type:{} with sign:{}",
    src_type.base_type, src_type.is_signed, base_type, is_signed);

    int dest_id = src_id;
    if(do_phy_conv && !dest_type.is_void() && !(base_type == src_type.get_simple_type().first && is_signed == src_type.get_simple_type().second))
        dest_id = code_gen::call_code_gen(fn_intf, &Ifunc_translation::type_cast, src_id, base_type, is_signed);
    src_type = dest_type;

    return dest_id;
}