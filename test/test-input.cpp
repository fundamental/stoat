#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))
void other_thing(void);

class TestClass
{
    void bar(void) {other_thing();};
    void REALTIME foo(void) {bar();};
    void REALTIME entry(void) { foo(); baz(); }
    void NREALTIME baz(void) {};
};

