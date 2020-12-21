#ifndef _ANGEL_BUFFER_H
#define _ANGEL_BUFFER_H

#include <vector>
#include <algorithm>
#include <string>

#include <sys/uio.h>

namespace angel {

class buffer {
public:
    buffer() : buf(init_size) {  }
    explicit buffer(size_t size) : buf(size) {  }
    ~buffer() {  }
    char *begin() { return &*buf.begin(); }
    char *peek() { return begin() + read_index; }
    size_t prependable() const { return read_index; }
    size_t readable() const { return write_index - read_index; }
    size_t writeable() const { return buf.size() - write_index; }
    void append(const char *data, size_t len)
    {
        make_space(len);
        std::copy(data, data + len, buf.begin() + write_index);
        write_index += len;
    }
    const char *c_str()
    {
        make_space(1);
        buf[write_index] = '\0';
        return peek();
    }
    int read_fd(int fd);
    // find [pattern] from [pos(s)]
    // if not found, return -1; else return pos(pattern[0])
    int find(char *s, const char *pattern)
    {
        const char *p = std::search(
                s, begin() + write_index, pattern, pattern + strlen(pattern));
        return p == begin() + write_index ? -1 : p - s;
    }
    int find(const char *pattern)
    {
        return find(peek(), pattern);
    }
    int find_crlf() { return find("\r\n"); }
    int find_lf() { return find("\n"); }
    // 跳过已读的数据
    void retrieve(size_t len)
    {
        if (len < readable())
            read_index += len;
        else
            read_index = write_index = 0;
    }
    void retrieve_all() { retrieve(readable()); }
    void swap(buffer& other)
    {
        buf.swap(other.buf);
        std::swap(read_index, other.read_index);
        std::swap(write_index, other.write_index);
    }
    bool starts_with(const std::string& s)
    {
        return readable() >= s.size() && memcmp(peek(), s.data(), s.size()) == 0;
    }
    char& operator[](size_t idx)
    {
        return buf[idx];
    }
private:
    // 内部腾挪
    void make_space(size_t len)
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

    static const size_t init_size = 1024;
    std::vector<char> buf;
    size_t read_index = 0;
    size_t write_index = 0;
};

}

#endif // _ANGEL_BUFFER_H
