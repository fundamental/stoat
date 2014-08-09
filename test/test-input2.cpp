#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))
static int stuffy_stuff(int i)
{
    return i;
}

static void undefined_and_unused(void);
void undefined_and_used(void);
static void unused(void)
{
}

void REALTIME other_thing(void)
{
    int i = 32;
    stuffy_stuff(i);
    undefined_and_used();
}
