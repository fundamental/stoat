class AA
{
    public:
        virtual ~AA(){}
};

class A:public AA
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
        virtual void mB4(){mB2();}
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

class X
{
    public:
        virtual ~X() {}
        virtual void mX(){}
};

class Y
{
    public:
        virtual ~Y() {}
};

class E: public X, public Y
{
    public:
        virtual ~E() {}
        virtual void mE1() {}
        virtual void mE2()=0; 
        virtual void mE3() {}
};

class F:public D, public E
{
    public:
        virtual ~F() {}
        virtual void mB1() {mF2();}
        virtual void mF1() {}
        virtual void mF2() {}
        virtual void mE2() {}
};

int main()
{
    D *d = new F;
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

void foo_evil2(B*b) __attribute__((annotate("realtime")))
{
    b->mB4();
}

void foo_evil3(F *f) __attribute__((annotate("realtime")))
{
    f->mF1();
}
