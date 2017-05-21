#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("non-realtime")))

void bb(void) NREALTIME
{
}

void a(void) REALTIME
{
    bb();
}
