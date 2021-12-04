#ifndef _ANGEL_BUFFER_H
#define _ANGEL_BUFFER_H

#include <vector>
#include <algorithm>
#include <string>

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
    size_t prependable() const { return read_index; }
    size_t readable() const { return write_index - read_index; }
    size_t writeable() const { return buf.size() - write_index; }
    void append(const char *data, size_t len);
    const char *c_str();
    int read_fd(int fd);
    // return index-pos if found else -1
    int find(char *pos, const char *pattern);
    int find(const char *pattern)
    {
        return find(peek(), pattern);
    }
    int find_crlf() { return find("\r\n"); }
    int find_lf() { return find("\n"); }
    // retrieve read data, peek() += len
    void retrieve(size_t len);
    void retrieve_all() { retrieve(readable()); }
    void swap(buffer& other);
    bool starts_with(const std::string& s)
    {
        return readable() >= s.size() && memcmp(peek(), s.data(), s.size()) == 0;
    }
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
