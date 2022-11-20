#include <angel/signal.h>

#include <unistd.h>

#include <iostream>

static size_t id1, id2;
static int called1 = 0, called2 = 0;

void signal_handler1()
{
    std::cout << "signal_handler1\n";
    if (++called1 >= 2) angel::cancel_signal(id1);
}

void signal_handler2()
{
    std::cout << "signal_handler2\n";
    if (++called2 >= 4) angel::cancel_signal(id2);
}

int main()
{
    id1 = angel::add_signal(SIGINT, signal_handler1);
    id2 = angel::add_signal(SIGINT, signal_handler2);
    for ( ; ; ) pause();
}
