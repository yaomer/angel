#include <angel/config.h>

#ifdef ANGEL_HAVE_SELECT

#include "select.h"

#include <angel/evloop.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

select_base_t::select_base_t()
{
    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    fds.reserve(EVLIST_INIT_SIZE);
    maxfd = -1;
    set_name("select");
}

select_base_t::~select_base_t()
{
}

void select_base_t::add(int fd, int events)
{
    if (events & Read) FD_SET(fd, &rdset);
    if (events & Write) FD_SET(fd, &wrset);

    if (fds.emplace(fd).second) {
        maxfd = std::max(maxfd, fd);
    }
}

void select_base_t::remove(int fd, int events)
{
    if (events & Read) FD_CLR(fd, &rdset);
    if (events & Write) FD_CLR(fd, &wrset);

    if (!FD_ISSET(fd, &rdset) && !FD_ISSET(fd, &wrset)) {
        if (fd == maxfd) maxfd--;
        fds.erase(fd);
    }
}

int select_base_t::wait(evloop *loop, int64_t timeout)
{
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    // We cannot pass rdset and wrset directly to select(),
    // because select() modifies them to hold the active fds
    // and we need to pass it a copy.
    FD_COPY(&rdset, &_rdset);
    FD_COPY(&wrset, &_wrset);
    if (timeout > 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
    }
    // select() examins the descriptors from 0 through nfds-1 in the descriptor sets.
    // (Example: If you have set two file descriptors "4" and "17", nfds should
    // not be "2", but rather "17 + 1" or "18".)
    //
    // nfds cannot be greater than FD_SETSIZE(usually 1024).
    //
    int nfds = maxfd + 1;
    assert(nfds >= 0);
    int nevents = select(nfds, &_rdset, &_wrset, nullptr,
            timeout >= 0 ? &tv : nullptr);
    int retval = nevents;
    if (nevents > 0) {
        for (auto& fd : fds) {
            int revs = 0;
            if (FD_ISSET(fd, &_rdset)) revs |= Read;
            if (FD_ISSET(fd, &_wrset)) revs |= Write;
            auto chl = loop->search_channel(fd);
            chl->set_trigger_events(revs);
            loop->active_channels.emplace_back(chl);
            if (revs && --nevents == 0)
                break;
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("select: %s", strerrno());
    }
    return retval;
}

}

#endif
