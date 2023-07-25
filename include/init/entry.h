#pragma once

//This file should only be included by entry.cpp

#ifdef SIMDEBUG
    #include "debug.h"
#endif

int app_start(int, char**);