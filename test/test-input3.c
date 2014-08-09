
//Function prototype (eg from a header)
void prototype_only(void);

void trivial_function(void)
{}

void calling_function(void)
{
    prototype_only();
    trivial_function();
}
