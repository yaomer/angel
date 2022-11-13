#include <angel/evloop.h>

#include <iostream>

angel::evloop *g_loop;

static size_t id1, id2;
static int called1 = 0, called2 = 0;

void signal_handler1()
{
    std::cout << "signal_handler1\n";
    if (++called1 >= 2) g_loop->cancel_signal(id1);
}

void signal_handler2()
{
    std::cout << "signal_handler2\n";
    if (++called2 >= 4) g_loop->cancel_signal(id2);
}

int main()
{
    angel::evloop loop;
    g_loop = &loop;

    id1 = loop.add_signal(SIGINT, signal_handler1);
    id2 = loop.add_signal(SIGINT, signal_handler2);

    loop.run();
}
