#include <functional>
#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))

typedef int (*callback_t)(int, void*);

void REALTIME make_realtime(std::function<int(int,void*)> cb, void*)
{
    cb(0, ((void*)0));
}

void NREALTIME malloc_of_doooooooooom();

void REALTIME tmp(void)
{
    ([](void){malloc_of_doooooooooom();})();
}

int main()
{
    std::function<int(int,void*)> foobar = [](int, void*){malloc_of_doooooooooom();return 1;};
    make_realtime(foobar, 0);
}
