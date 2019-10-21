#ifndef _ANGEL_BUFFER_H
#define _ANGEL_BUFFER_H

#include <vector>
#include <algorithm>

namespace Angel {

class Buffer {
public:
    Buffer() : _buf(init_size) {  }
    explicit Buffer(size_t size) : _buf(size) {  }
    ~Buffer() {  }
    char *begin() { return &*_buf.begin(); }
    char *peek() { return begin() + _readindex; }
    size_t prependable() const { return _readindex; }
    size_t readable() const { return _writeindex - _readindex; }
    size_t writeable() const { return _buf.size() - _writeindex; }
    void append(const char *data, size_t len)
    {
        makeSpace(len);
        std::copy(data, data + len, _buf.begin() + _writeindex);
        _writeindex += len;
    }
    // 内部腾挪
    void makeSpace(size_t len)
    {
        if (len > writeable()) {
            if (len <= writeable() + prependable()) {
                size_t readBytes = readable();
                std::copy(peek(), peek() + readBytes, begin());
                _readindex = 0;
                _writeindex = _readindex + readBytes;
            } else
                _buf.resize(_writeindex + len);
        }
    }
    // 返回C风格字符串
    const char *c_str()
    {
        makeSpace(1);
        _buf[_writeindex] = '\0';
        return peek();
    }
    int findStr(char *s, const char *pattern)
    {
        const char *p = std::search(s, begin() + _writeindex,
                pattern, pattern + strlen(pattern));
        return p == begin() + _writeindex ? -1 : p - s;
    }
    int findStr(const char *pattern)
    {
        return findStr(peek(), pattern);
    }
    // 返回\r\n在Buffer中第一次出现的位置，没出现返回-1
    int findCrlf() { return findStr("\r\n"); }
    int findLf() { return findStr("\n"); }
    // 跳过已读的数据
    void retrieve(size_t len)
    {
        if (len < readable())
            _readindex += len;
        else
            _readindex = _writeindex = 0;
    }
    void retrieveAll() { retrieve(readable()); }
    int readFd(int fd);
    void swap(Buffer& _buffer)
    {
        _buf.swap(_buffer._buf);
        std::swap(_readindex, _buffer._readindex);
        std::swap(_writeindex, _buffer._writeindex);
    }
    char& operator[](size_t idx) { return _buf[idx]; }
    bool strcmp(const char *s)
    {
        size_t len = strlen(s);
        return readable() >= len && strncmp(peek(), s, len) == 0;
    }
    bool strcasecmp(const char *s)
    {
        size_t len = strlen(s);
        return readable() >= len && strncasecmp(peek(), s, len) == 0;
    }
    static const size_t init_size = 1024;
private:
    std::vector<char> _buf;
    size_t _readindex = 0;
    size_t _writeindex = 0;
};
} // Angel

#endif // _ANGEL_BUFFER_H
