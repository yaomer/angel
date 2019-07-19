#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

namespace Angel {

const char *strerrno();
const char *strerr(int err);

size_t getThreadId();
const char *getThreadIdStr();
}

#endif // _ANGEL_UTIL_H
