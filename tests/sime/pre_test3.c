//String and constant test

#if 0
#include <pre_test1.c>
#define PROCESS 25
#define PROCESS_THIS PROCESS 'PROCESS' 'PROCESS
#define INVOKE(a, b) a##b
 "PROCESS INVOKE(PROCESS, _THIS)
#endif

#if 1

#include <pre_test2.c>
 INVOKE(PROCESS, _THIS)
#endif


