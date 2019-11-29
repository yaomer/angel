#ifndef _ANGEL_UTIL_H
#define _ANGEL_UTIL_H

namespace Angel {

const char *strerrno();
const char *strerr(int err);

size_t getThreadId();
const char *getThreadIdStr();

int64_t nowMs();
int64_t nowUs();

}

#endif // _ANGEL_UTIL_H
