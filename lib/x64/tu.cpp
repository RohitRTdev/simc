#include "lib/code-gen.h"
#include "lib/x64/x64.h"

void filter_type(c_type& type, bool& is_signed) {
    if(type == C_PTR) {
        type = C_LONG;
        is_signed = false;
    }
}

void filter_type(c_type& type) {
    if(type == C_PTR) {
        type = C_LONG;
    }
}


x64_tu::x64_tu() : global_var_id(0) {
    x64_func::new_label_id = 0;
}


const c_var& x64_tu::fetch_global_variable(int id) const {
    return globals[id-1];
}

int x64_tu::declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static) {
    filter_type(type, is_signed);
    c_var var{};
    var.name = name;
    var.is_signed = is_signed;
    var.type = type;
    var.is_static = is_static;
    globals.push_back(var);

    sim_log_debug("Declared global variable:{} with type id:{}", name, type);

    return ++global_var_id;
}

int x64_tu::declare_global_variable(std::string_view name, c_type type, bool is_signed, bool is_static, std::string_view constant) {
    int var_id = declare_global_variable(name, type, is_signed, is_static);
    globals[var_id-1].value = constant;

    sim_log_debug("Initializing var:{} with value:{}", name, constant);

    return var_id;
}

int x64_tu::declare_global_mem_variable(std::string_view name, bool is_static, size_t mem_var_size) {
    c_var var{};
    var.name = name;
    var.mem_var_size = mem_var_size; 
    var.type = C_LONG;
    var.is_signed = false;
    var.is_static = is_static;
    globals.push_back(var);

    sim_log_debug("Declared global memory variable:{} with size:{}", name, mem_var_size);

    return ++global_var_id;
}

Ifunc_translation* x64_tu::add_function(std::string_view name, c_type ret_type, bool is_signed, bool is_static) {
    filter_type(ret_type, is_signed);
    auto fn = new x64_func(name, this, ret_type, is_signed);

    declare_global_variable(name, ret_type, is_signed, is_static);
    globals[global_var_id-1].is_fn = true;
    fn_list.push_back({fn, global_var_id - 1});

    return fn;
}

void x64_tu::generate_code() {
    static const char* global_type_names[] = {"long", "byte", "quad", "word"};
    
    std::string bss_section;
    std::string data_section;
    
    for(const auto& var : globals) {
        if(var.is_fn)
            continue;
        if(var.value.size()) {
            data_section.append(fmt::format(LINE(".global {}"), var.name));
            data_section.append(fmt::format(LINE("{}:\n\t.{} {}"), var.name, base_type_size(var.type), var.value));
        }
        else {
            size_t var_size = var.mem_var_size.has_value() ? var.mem_var_size.value() : base_type_size(var.type);
            bss_section.append(INSTRUCTION(".{}comm {}, {}", var.is_static ? "l" : "", var.name, var_size));
        }
    }

    write_segment<DATA>(data_section);
    write_segment<BSS>(bss_section);

    if(fn_list.size()) {
        add_inst_to_code(LINE(".section .text"));
    }
    for(auto& [fn, var] : fn_list) {
        fn->generate_code();
        if(!globals[var].is_static) {
            std::string_view name = fn->fetch_fn_name();
            add_inst_to_code(fmt::format(LINE(".global {}"), name));
            add_inst_to_code(fmt::format(LINE(".type {}, @function"), name));
        }
        add_inst_to_code(fn->fetch_code());
        add_inst_to_code(LINE());
    }
}

Itranslation* create_translation_unit() {
    auto unit = new x64_tu();

    return unit;
}

x64_tu::~x64_tu() {
    for(auto [fn, size]: fn_list) {
        sim_log_debug("Deleting function instances");
        delete fn;
    }
}
