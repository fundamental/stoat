#include <cmath>
#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))

void REALTIME test_functions()
{
    float f = 23.3;
    sinf(f);
    cosf(f);
    powf(f,f);
}

int main()
{
}
