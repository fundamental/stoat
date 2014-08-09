#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))

typedef int (*callback_t)(int, void*);

void make_realtime(callback_t REALTIME call, void*)
{
}

void NREALTIME malloc_of_doooooooooom();

class Runner
{
    public:
        int process(int, void*)
        {
            malloc_of_doooooooooom();
            return 1;
        }

        static int _process(int i, void* data)
        {
            Runner *ptr = (Runner*)(data);
            return ptr->process(i,data);
        }
};

int main()
{
    make_realtime(Runner::_process, new Runner);
}
