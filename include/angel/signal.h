#ifndef __ANGEL_SIGNAL_H
#define __ANGEL_SIGNAL_H

#include <signal.h>

#include <functional>

namespace angel {

typedef std::function<void()> signal_handler_t;

size_t add_signal(int signo, const signal_handler_t handler);
// Restore the default semantics of signo.
void cancel_signal(size_t id);
void ignore_signal(int signo);

}

#endif // __ANGEL_SIGNAL_H
