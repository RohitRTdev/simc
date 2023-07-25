#pragma once


#ifdef MSVC
    #ifdef BUILD_LIB
        #define DLL_ATTRIB __declspec(dllexport)
    #else
        #define DLL_ATTRIB __declspec(dllimport)
    #endif
#elif defined(GNU) || defined(Clang)
    #define DLL_ATTRIB __attribute__((visibility("default")))
#else
    #define DLL_ATTRIB
#endif