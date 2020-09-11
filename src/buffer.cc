#include "buffer.h"

using namespace angel;

namespace angel {

    thread_local char extrabuf[65536];
}

int buffer::read_fd(int fd)
{
    struct iovec iov[2];
    ssize_t writen = writeable();
    ssize_t n;

    iov[0].iov_base = begin() + write_index;
    iov[0].iov_len = writen;
    iov[1].iov_base = extrabuf;
    iov[1].iov_len = sizeof(extrabuf);

    if ((n = readv(fd, iov, 2)) > 0) {
        if (n <= writen)
            write_index += n;
        else {
            write_index += writen;
            append(extrabuf, n - writen);
        }
    }
    return n;
}
