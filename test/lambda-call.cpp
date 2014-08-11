#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))

void malloc_of_doooooooooom() NREALTIME ;

void some_realtime_function(void) REALTIME
{
    auto fn = ([](void){malloc_of_doooooooooom();});
    fn();
}
