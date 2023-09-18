#include "init/entry.h"

//Common entry point for all modules
int main(int argc, char** argv) {
    #ifdef SIMDEBUG
        init_logger();
        #ifdef INITDEBUGGER
            init_debugger(sim_logger);
        #endif
    #endif
    return app_start(argc, argv);
}