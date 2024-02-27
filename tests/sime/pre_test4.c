


//Process this block
#if 1

#define hehe 1

#endif

/*Don't process this block*/
#if 0

#define choke 10
#if 1
#endif
#endif
"Good

//Nested #if check
#define GREAT hehe choke hehe
GREAT
#if 0
Processed 1
#if 1
Processed  2
#if 0
Processed   3
#elif 0
Processed 5
#elif 1
Processed 6
#endif
#endif
#include <pre_test3.c>
#if 0
Processed  4
#else
Processed 7
#endif
#elif 1   
Processed 8
#endif

//Ifdef functionality check
#ifdef choke
choke is defined
#endif

#ifndef choke
Choke is not defined
#ifdef GREAT
Great is defined
#endif
#endif

//#if exp check
#if 2+3+4*2 > 50
Expression 1
#if 5+
Should not fail
#elif 3*=
No fail
#else agg
No
#endif
#elif 2+3+4*3 == 14
Expression 2
#elif 2+3+4*2 == 13
Expression 3
"String here"
#define hehehe
/*This should not get mentioned*/
//Hehe
#else 
/*Hehe 
 * There we go */
Expression 4
#endif
#ifdef hehehe
It works!! //Hehe
#endif
Preprocess successful
