#ifndef _ANGEL_EVENT_H
#define _ANGEL_EVENT_H

namespace angel {

    enum class event {
        read    = 01,
        write   = 02,
        error   = 04,
    };
}

#endif // _ANGEL_EVENT_H
