class A
{
    public:
        A(){}
        virtual ~A(){}
        virtual void method_b(void) = 0;
        virtual void method_c(void) = 0;
        virtual void method_d(void) = 0;
};

class B:virtual public A
{
    public:
        B(){};
        virtual ~B(){};
        virtual void method_b(void) {}
        virtual void method_c(void) {}
};

class C:virtual public A
{
    public:
        C(){}
        virtual ~C(){}
        virtual void method_b(void) {}
        virtual void method_c(void) {}
        virtual void method_d(void) {}
};

class D:virtual public B
{
    public:
        D(){}
        virtual ~D(){}
        virtual void method_d(void) {}
        C c;
};

void dummy()
{
    D d;
}

void test(A *a) __attribute__((annotate("realtime")))
{
    a->method_b();
    a->method_c();
    a->method_d();
}

