#ifndef _ANGEL_CONFIG_H
#define _ANGEL_CONFIG_H

#if defined (__APPLE__)
#define _ANGEL_HAVE_KQUEUE 1
#define _ANGEL_HAVE_POLL 1
#define _ANGEL_HAVE_SELECT 1
#endif

#if defined (__linux__)
#define _ANGEL_HAVE_EPOLL 1
#define _ANGEL_HAVE_POLL 1
#define _ANGEL_HAVE_SELECT 1
#endif

#endif // _ANGEL_CONFIG_H
