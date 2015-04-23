class A
{
    public:
        virtual ~A(){}
        virtual void mA1(){};
        virtual void mA2(){};
};

class B
{
    public:
        virtual ~B(){}
        virtual void mB1()=0;
        virtual void mB2()=0;
        virtual void mB3(){};
};

class C
{
    public:
        virtual ~C(){}
        virtual void mC1(){};
        virtual void mC2()=0;
        virtual void mC3()=0;
};

class D:public A, public B, public C
{
    public:
        virtual ~D(){};
        virtual void mB1(){};
        virtual void mB2() __attribute__((annotate("non-realtime"))) {} ;
        virtual void mC2(){};
        virtual void mC3(){};
        virtual void mD(){};
};

int main()
{
    D *d = new D;
    d->mA1();
    d->mA2();

    d->mB1();
    d->mB2();
    d->mB3();

    d->mC1();
    d->mC2();
    d->mC3();
    
    d->mD();

    //Look at the A case
    A *a = (A*)(d);
    a->mA1();
    a->mA2();
    
    //Look at the B case
    B *b = (B*)(d);
    b->mB1();
    b->mB2();
    b->mB3();
    
    //Look at the C case
    C *c = (C*)(d);
    c->mC1();
    c->mC2();
    c->mC3();
    return 0;
}

void foo_good(B*b) __attribute__((annotate("realtime")))
{
    b->mB1();
}

void foo_evil(B*b) __attribute__((annotate("realtime")))
{
    b->mB2();
}
