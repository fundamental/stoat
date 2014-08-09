#define RT __attribute__((annotate("realtime")))
#define nRT __attribute__((annotate("!realtime")))

class A
{
    public:
        virtual void method(void) = 0;
};

class B : public A
{
    public:
};

class C : public B
{
    public:
        virtual void method(void){};
        void other(void);
};

class D : public virtual C
{
    public:
        virtual void RT method(void) {};
};


int RT main()
{
    D d;
    A *a = &d;
    a->method();
}
