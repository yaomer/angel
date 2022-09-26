#include "config.h"

#ifdef ANGEL_HAVE_SELECT

#include "select.h"

#include <angel/evloop.h>
#include <angel/logger.h>
#include <angel/util.h>

namespace angel {

using namespace util;

static const size_t fds_init_size = 64;

select_base_t::select_base_t()
{
    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    fds.reserve(fds_init_size);
    maxfd = -1;
    set_name("select");
}

select_base_t::~select_base_t() = default;

void select_base_t::add(int fd, int events)
{
    fds.push_back(fd);
    indexs.emplace(fd, fds.size() - 1);
    change(fd, events);
    maxfd = std::max(maxfd, fd, std::greater<int>());
}

// To modify the event associated with the fd,
// you must first clear the originally registered fd from rdset and wrset,
// and then re-register, otherwise the same fd may be registered repeatedly.
void select_base_t::change(int fd, int events)
{
    FD_CLR(fd, &rdset);
    FD_CLR(fd, &wrset);
    if (events & Read)
        FD_SET(fd, &rdset);
    if (events & Write)
        FD_SET(fd, &wrset);
}

void select_base_t::remove(int fd, int events)
{
    UNUSED(events);
    auto it = indexs.find(fd);
    size_t end = fds.size() - 1;
    std::swap(fds[it->second], fds[end]);
    fds.pop_back();
    indexs.erase(fd);
    change(fd, 0);
    if (fd == maxfd)
        maxfd--;
}

int select_base_t::wait(evloop *loop, int64_t timeout)
{
    struct timeval tv;
    // We cannot pass rdset and wrset directly to select(),
    // because select() modifies them to hold the active fds
    // and we need to pass it a copy.
    fd_set rdset1, wrset1, errset;
    FD_ZERO(&rdset1);
    FD_ZERO(&wrset1);
    FD_ZERO(&errset);
    rdset1 = rdset;
    wrset1 = wrset;
    if (timeout > 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
    }
    int nevents = select(maxfd + 1, &rdset1, &wrset1, &errset,
            timeout > 0 ? &tv : nullptr);
    int rets = nevents;
    if (nevents > 0) {
        for (auto& it : fds) {
            int revs = 0;
            if (FD_ISSET(it, &rdset1))
                revs |= Read;
            if (FD_ISSET(it, &wrset1))
                revs |= Write;
            if (FD_ISSET(it, &errset))
                revs |= Error;
            auto chl = loop->search_channel(it);
            chl->set_trigger_events(revs);
            loop->active_channels.emplace_back(chl);
            if (revs && --nevents == 0)
                break;
        }
    } else if (nevents < 0) {
        if (errno != EINTR)
            log_error("select: %s", strerrno());
    }
    return rets;
}

}

#endif
