class A
{
    public:
    virtual void m1(){};
};

class B
{
    public:
    virtual void m2(){};
};

class C:public A, public B
{
    public:
    virtual void m3(){};
};

class D:public C
{
    public:
    virtual void m4()__attribute__((annotate("non-realtime"))){};
};

void foo(C*c)__attribute__((annotate("realtime")))
{
    c->m2();
}

int main()
{
    new D;
    return 0;
}
