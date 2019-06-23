#ifndef _ANGEL_NONCOPYABLE_H
#define _ANGEL_NONCOPYABLE_H

namespace Angel {

class noncopyable {
public:
    noncopyable() = default; 
    ~noncopyable() = default;
private:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};
}

#endif // _ANGEL_NONCOPYABLE_H
