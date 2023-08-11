#include "lib/code-gen.h"
#include "lib/x64/x64.h"

x64_tu::x64_tu() : global_var_id(0)
{}

std::string_view x64_tu::fetch_global_variable(int id) const {
    return globals[id-1].name;
}

int x64_tu::declare_global_variable(const std::string& name, c_type type) {
    c_var var;
    var.name = name;
    var.type = type;
    globals.push_back(var);

    sim_log_debug("Declared global variable:{} with type id:{}", name, type);

    return ++global_var_id;
}

int x64_tu::declare_global_variable(const std::string& name, c_type type, std::string_view constant) {
    int var_id = declare_global_variable(name, type);
    globals[var_id-1].value = constant;

    sim_log_debug("Declared global variable:{} with type id:{} and value:{}", name, type, constant);

    return var_id;
}

Ifunc_translation* x64_tu::add_function(const std::string& name) {
    auto fn = new x64_func(name, this, global_var_id);
    fn_list.push_back({fn, global_var_id});

    return fn;
}

void x64_tu::generate_code() {

    static const char* global_type_names[] = {"long", "byte", "quad"};
    static const char* global_type_sizes[] = {"4", "1", "8"};
    
    std::string bss_section = LINE(".section .bss");
    std::string data_section = LINE(".section .data");
    bool data_section_present = false, bss_section_present = false;
    
    for(const auto& var : globals) {
        if(var.value.size()) {
            data_section.append(fmt::format(LINE(".global {}"), var.name));
            data_section.append(fmt::format(LINE("{}:\n\t.{} {}"), var.name, global_type_names[var.type], var.value));
            data_section_present = true;
        }
        else {
            bss_section.append(INSTRUCTION(".comm {}, {}", var.name, global_type_sizes[var.type]));
            bss_section_present = true;
        }
    }

    if(data_section_present)
        add_inst_to_code(data_section);
   
    if(bss_section_present)
        add_inst_to_code(bss_section);


    if(fn_list.size()) {
        add_inst_to_code(LINE(".section .text"));
    }
    for(auto& [fn, var] : fn_list) {
        fn->generate_code();
        add_inst_to_code(fmt::format(LINE(".global {}"), fn->fetch_fn_name()));
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
