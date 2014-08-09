#include <stdlib.h>
#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))

int REALTIME main()
{
    malloc(123);
    return 0;
}
