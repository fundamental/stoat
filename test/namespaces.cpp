namespace A {
    class aa {
        public:
        virtual int doStuff() __attribute__((annotate("realtime"))) {return 1;}
    };
}

namespace B {
    class bb: public A::aa{
        public:
        int doStuff() __attribute__((annotate("realtime"))) {return 2;}
    };
}

class cc:public B::bb
{
    public:
    int doStuff() __attribute__((annotate("non-realtime"))) {return 3;}
};

int main() __attribute__((annotate("realtime")))
{
    B::bb *B = new cc;
    B->doStuff();
    return 0;
}
