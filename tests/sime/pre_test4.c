


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
