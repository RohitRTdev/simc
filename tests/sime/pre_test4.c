


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
#if 1
Processed 1
#if 1
Processed  2
#if 0
Processed   3
#endif
#endif
#include <pre_test3.c>
#if 2
Processed  4
#endif
#endif
