#include <angel/buffer.h>

#include <sys/uio.h>

namespace angel {

static const size_t init_size = 1024;

buffer::buffer() : buf(init_size)
{
}

buffer::buffer(size_t size) : buf(size)
{
}

buffer::~buffer() = default;

void buffer::make_space(size_t len)
{
    if (len > writeable()) {
        if (len <= writeable() + prependable()) {
            size_t read_bytes = readable();
            std::copy(peek(), peek() + read_bytes, begin());
            read_index = 0;
            write_index = read_index + read_bytes;
        } else
            buf.resize(write_index + len);
    }
}

void buffer::append(std::string_view s)
{
    append(s.data(), s.size());
}

void buffer::append(const char *data, size_t len)
{
    make_space(len);
    std::copy(data, data + len, buf.begin() + write_index);
    write_index += len;
}

void buffer::retrieve(size_t len)
{
    if (len < readable()) {
        read_index += len;
    } else {
        read_index = write_index = 0;
    }
}

const char *buffer::c_str()
{
    make_space(1);
    buf[write_index] = '\0';
    return peek();
}

void buffer::swap(buffer& other)
{
    buf.swap(other.buf);
    std::swap(read_index, other.read_index);
    std::swap(write_index, other.write_index);
}

static thread_local char extrabuf[65536];

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
        if (n <= writen) {
            write_index += n;
        } else {
            write_index += writen;
            append(extrabuf, n - writen);
        }
    }
    return n;
}

}
