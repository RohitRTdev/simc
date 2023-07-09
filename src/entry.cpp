#include "common.h"

//Common entry point for all modules
int main(int argc, char** argv) {

    int ret_code = 0;
    #ifdef SIMDEBUG
        init_logger();
        ret_code = app_start(argc, argv);
    #else
        ret_code = app_start(argc, argv);
    #endif

    return ret_code;

}