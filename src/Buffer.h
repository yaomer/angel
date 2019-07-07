#ifndef _ANGEL_BUFFER_H
#define _ANGEL_BUFFER_H

#include <vector>
#include <algorithm>

namespace Angel {

class Buffer {
public:
    Buffer() : _buf(INIT_SIZE) {  }
    ~Buffer() {  }
    static const size_t INIT_SIZE = 1024;
    static const char _crlf[];
    char *begin() { return &*_buf.begin(); }
    char *peek() { return begin() + _readindex; }
    size_t prependable() const { return _readindex; }
    size_t readable() const { return _writeindex - _readindex; }
    size_t writeable() const { return _buf.capacity() - _writeindex; }
    void append(const char *data, size_t len)
    {
        makeSpace(len);
        _buf.insert(_buf.begin() + _writeindex, data, data + len);
        _writeindex += len;
    }
    // 内部腾挪
    void makeSpace(size_t len)
    {
        // 有足够的腾挪空间
        if (len < writeable() && writeable() + prependable() > len) {
            size_t readn = readable();
            std::copy(peek(), peek() + readn, begin());
            _readindex = 0;
            _writeindex = _readindex + readn;
        }
    }
    // 返回C风格字符串
    const char *c_str() 
    { 
        append("\0", 1); 
        _writeindex--; 
        return peek(); 
    }
    int findStr(const char *s, size_t len)
    {
        const char *pattern = 
            std::search(peek(), begin() + _writeindex, s, s + len);
        return pattern == begin() + _writeindex ? 
            -1 : pattern - peek();
    }
    // 返回\r\n在Buffer中第一次出现的位置，没出现返回-1
    int findCrlf() { return findStr("\r\n", 2); }
    int findLf() { return findStr("\n", 1); }
    // 跳过已读的数据
    void retrieve(size_t len)
    {
        if (len < readable())
            _readindex += len;
        else
            _readindex = _writeindex = 0;
    }
    void retrieveAll() { retrieve(readable()); }
    // 回退数据
    void back(size_t len)
    {
        if (len < prependable())
            _readindex -= len;
        else
            _readindex = 0;
    }
    int readFd(int fd);
    void swap(Buffer& _buffer)
    {
        _buf.swap(_buffer._buf);
        std::swap(_readindex, _buffer._readindex);
        std::swap(_writeindex, _buffer._writeindex);
    }
    void skipSpaceOfStart()
    {
        int i = 0;
        while (i < readable() && isspace(_buf[i]))
            i++;
        retrieve(i);
    }
    char& operator[](size_t idx) { return _buf[idx]; }
private:
    std::vector<char> _buf;
    size_t _readindex = 0;
    size_t _writeindex = 0;
};
} // Angel

#endif // _ANGEL_BUFFER_H
