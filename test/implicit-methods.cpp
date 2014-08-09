
class bar
{
    public:
    ~bar(void)
    {
        new int;
    }
};

class foo
{
    public:
    bar boo;
};

int blam(void) __attribute__((annotate("realtime")))
{
    foo fooo;
    return 0;
}
