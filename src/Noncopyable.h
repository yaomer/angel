#ifndef _ANGEL_NONCOPYABLE_H
#define _ANGEL_NONCOPYABLE_H

namespace Angel {

class Noncopyable {
public:
    Noncopyable() {  };
    ~Noncopyable() {  };
private:
    Noncopyable(const Noncopyable&);
    const Noncopyable& operator=(const Noncopyable&);
};
}

#endif // _ANGEL_NONCOPYABLE_H
