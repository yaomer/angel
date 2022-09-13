#ifndef _ANGEL_BUFFER_H
#define _ANGEL_BUFFER_H

#include <vector>
#include <algorithm>
#include <string>
#include <string_view>

namespace angel {

//
// +------------------------+
// | avail | xxxxxx | avail |
// +------------------------+
//         r        w
class buffer {
public:
    buffer();
    explicit buffer(size_t size);
    ~buffer();

    char *begin() { return &*buf.begin(); }
    char *peek() { return begin() + read_index; }
    char *end() { return peek() + readable(); }
    size_t prependable() const { return read_index; }
    size_t readable() const { return write_index - read_index; }
    size_t writeable() const { return buf.size() - write_index; }

    int read_fd(int fd);

    void append(std::string_view s);
    void append(const char *data, size_t len);

    // retrieve read data, peek() += len
    void retrieve(size_t len);
    void retrieve_all() { retrieve(readable()); }
    const char *c_str();

    bool starts_with(std::string_view s)
    {
        return readable() >= s.size() && memcmp(peek(), s.data(), s.size()) == 0;
    }
    int find(std::string_view pattern)
    {
        return find(peek(), pattern);
    }
    int find_crlf() { return find("\r\n"); }
    int find_lf() { return find("\n"); }
    // return index-pos if found else -1
    int find(char *pos, std::string_view pattern);

    void swap(buffer& other);
    char& operator[](size_t idx)
    {
        return buf[idx];
    }
private:
    void make_space(size_t len);

    std::vector<char> buf;
    size_t read_index = 0;
    size_t write_index = 0;
};

}

#endif // _ANGEL_BUFFER_H
