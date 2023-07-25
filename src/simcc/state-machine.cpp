#include "compiler/state-machine.h"
#include "compiler/ast-ops.h"
#include "debug-api.h"

state_machine::state_machine(): cur_state(PARSER_START), token_stream(nullptr), advance_token(true), token_idx(0), num_states(0){

}

void state_machine::set_token_stream(std::vector<token>& tok_stream) {
    token_stream = &tok_stream;
}

token* state_machine::fetch_token() {
    return &(*token_stream)[token_idx++];
}
   
std::unique_ptr<ast> state_machine::fetch_parser_stack() {
    auto node = std::move(parser_stack.top());
    parser_stack.pop();
    return node;
}

void state_machine::switch_state(const parser_states new_state) {
    sim_log_debug("Switching state to {}", state_path[new_state].name);
    cur_state = new_state;
}

void state_machine::stop_token_fetch() {
    sim_log_debug("Continuing with same token");
    advance_token = false;
}

void state_machine::define_state(const std::string& state_name, parser_states next_state, state_machine::fptr pcheck, parser_states alt_state, bool shift, reducer reduce_this, const char* error_string, parser_states push_state) {

    bool fail_on_error = (error_string != nullptr);
    std::string _error_string;
    if(fail_on_error)
        _error_string = error_string;

    _state val{static_cast<parser_states>(num_states++), next_state, alt_state, pcheck, state_name, shift, reduce_this, fail_on_error, _error_string, push_state, false};

    state_path.push_back(val);

}

void state_machine::define_special_state(const std::string& state_name, state_machine::fptr pcheck, reducer special_reduce, parser_states alt_state, const char* error_string) {
    define_state(state_name, PARSER_INVALID, pcheck, alt_state, false, special_reduce);
    state_path[num_states - 1].is_special = true;
}

void state_machine::define_reduce_state(const std::string& state_name, parser_states next_state, reducer reduce_this) {
    _state val{static_cast<parser_states>(num_states++), next_state, PARSER_ERROR, nullptr, state_name, false, reduce_this, false, std::string(), PARSER_INVALID, false};

    state_path.push_back(val);
}

void state_machine::start() {

    token* tok = nullptr;

    auto alternator = [&] {
        if (!state_path[cur_state].fail_to_error) {
            cur_state = state_path[cur_state].alt_state;
            sim_log_debug("Moving to alternate state:{}", state_path[cur_state].name);
            advance_token = false;
        }
        else {
            sim_log_error(state_path[cur_state].error_string);
        }
    };

    auto mover = [&] {
        cur_state = state_path[cur_state].next_state;
        sim_log_debug("Moving to state:{}", state_path[cur_state].name);
        advance_token = true;
    };

    auto check_and_move = [&] {
        if ((tok->*(state_path[cur_state].pcheck))()) {
            mover();
        }
        else
            alternator();
    };

    auto shifter = [&] {
        if ((tok->*(state_path[cur_state].pcheck))()) {
            sim_log_debug("Shifting token");
            parser_stack.push(create_ast_token(tok));
            mover();
        }
        else
            alternator();
    };

    auto reduce = [&] {
        if ((tok->*(state_path[cur_state].pcheck))()) {
            state_path[cur_state].reduce_this(this);
            mover();
        }
        else
            alternator();
    };

    auto move_and_reduce = [&] {
        state_path[cur_state].reduce_this(this);
        cur_state = state_path[cur_state].next_state;
        sim_log_debug("Moving to state:{}", state_path[cur_state].name);
        advance_token = false;
    };

    auto push = [&] {
        if (state_path[cur_state].push_state != PARSER_INVALID
        && (tok->*(state_path[cur_state].pcheck))()) {
            auto push_state = state_path[cur_state].push_state;
            if(push_state < num_states)
                sim_log_debug("Pushing state:{}", state_path[push_state].name);
            state_stack.push(state_path[cur_state].push_state);
        }
    };

    auto special = [&] {
        if ((tok->*(state_path[cur_state].pcheck))()) {
            sim_log_debug("Calling special reduce function");
            advance_token = true;
            state_path[cur_state].reduce_this(this);
        }
        else
            alternator();
    };

    cur_state = PARSER_START;
    advance_token = true;
    token_idx = 0;

    while ((advance_token == false || token_idx < token_stream->size()) && cur_state != PARSER_END) {
        sim_log_debug("In state:{}", state_path[cur_state].name);
        if(advance_token) {
            sim_log_debug("Fetching new token");
            tok = fetch_token();
        }
        tok->print();

        if (state_path[cur_state].pcheck) {
            push();
 
            if (state_path[cur_state].is_special) {
                special();
            }
            else {
                if (state_path[cur_state].shift)
                    shifter();
                else if (state_path[cur_state].reduce_this)
                    reduce();
                else
                    check_and_move();
            }
        }
        else {
            move_and_reduce();
        }
        
    }


}