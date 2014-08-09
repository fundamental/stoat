#define REALTIME __attribute__((annotate("realtime")))
#define NREALTIME __attribute__((annotate("!realtime")))

typedef void (*rt_callback_t)(void);

void REALTIME function(void)
{}

void NREALTIME bad_function(void)
{}

int REALTIME main()
{
    const char *not_a_function_pointer;
    int         me_neither;
    void       *hey_look_some_function_pointers_are_below;

    not_a_function_pointer = "string";
    me_neither = 2;
    hey_look_some_function_pointers_are_below = &me_neither;


    rt_callback_t fn;
    fn = function;
    fn = bad_function;
    fn();
    void (*func)(void) = bad_function;
}
