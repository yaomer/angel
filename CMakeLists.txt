cmake_minimum_required (VERSION 3.1)

set (CMAKE_CXX_FLAGS "-std=c++11 -Wall -O2")

set (CMAKE_DIR /usr/local/share/cmake/Modules)

include (${CMAKE_DIR}/CheckFunctionExists.cmake)

CHECK_FUNCTION_EXISTS (poll _ANGEL_HAVE_POLL)
CHECK_FUNCTION_EXISTS (epoll_wait _ANGEL_HAVE_EPOLL)
CHECK_FUNCTION_EXISTS (kqueue _ANGEL_HAVE_KQUEUE)
CHECK_FUNCTION_EXISTS (select _ANGEL_HAVE_SELECT)

set (SRC_DIR ./src)

configure_file (
    "${SRC_DIR}/config.h.in"
    "${SRC_DIR}/config.h"
)

set (SRC_HEADERS
    ${SRC_DIR}/Acceptor.h
    ${SRC_DIR}/Connector.h
    ${SRC_DIR}/Buffer.h
    ${SRC_DIR}/Channel.h
    ${SRC_DIR}/EventLoop.h
    ${SRC_DIR}/EventLoopThread.h
    ${SRC_DIR}/EventLoopThreadPool.h
    ${SRC_DIR}/TcpConnection.h
    ${SRC_DIR}/InetAddr.h
    ${SRC_DIR}/Socket.h
    ${SRC_DIR}/SockOps.h
    ${SRC_DIR}/TcpServer.h
    ${SRC_DIR}/TcpClient.h
    ${SRC_DIR}/TimeStamp.h
    ${SRC_DIR}/Timer.h
    ${SRC_DIR}/Signaler.h
    ${SRC_DIR}/ThreadPool.h
    ${SRC_DIR}/LogStream.h
    ${SRC_DIR}/Poller.h
    ${SRC_DIR}/Id.h
    ${SRC_DIR}/noncopyable.h
    ${SRC_DIR}/decls.h
)

set (SRC_FILES 
    ${SRC_DIR}/EventLoop.cc
    ${SRC_DIR}/EventLoopThread.cc
    ${SRC_DIR}/EventLoopThreadPool.cc
    ${SRC_DIR}/Channel.cc
    ${SRC_DIR}/Buffer.cc
    ${SRC_DIR}/TcpConnection.cc
    ${SRC_DIR}/InetAddr.cc
    ${SRC_DIR}/Socket.cc
    ${SRC_DIR}/SockOps.cc
    ${SRC_DIR}/Acceptor.cc
    ${SRC_DIR}/Connector.cc
    ${SRC_DIR}/TcpServer.cc
    ${SRC_DIR}/TcpClient.cc
    ${SRC_DIR}/TimeStamp.cc
    ${SRC_DIR}/Timer.cc
    ${SRC_DIR}/Signaler.cc
    ${SRC_DIR}/ThreadPool.cc
    ${SRC_DIR}/LogStream.cc
    ${SRC_DIR}/Logger.cc
)

if (_ANGEL_HAVE_POLL)
    list(APPEND SRC_FILES ${SRC_DIR}/Poll.cc)
    list(APPEND SRC_HEADERS ${SRC_DIR}/Poll.h)
endif()

if (_ANGEL_HAVE_EPOLL)
    list(APPEND SRC_FILES ${SRC_DIR}/Epoll.cc)
    list(APPEND SRC_HEADERS ${SRC_DIR}/Epoll.h)
endif()

if (_ANGEL_HAVE_KQUEUE)
    list(APPEND SRC_FILES ${SRC_DIR}/Kqueue.cc)
    list(APPEND SRC_HEADERS ${SRC_DIR}/Kqueue.h)
endif()

if (_ANGEL_HAVE_SELECT)
    list(APPEND SRC_FILES ${SRC_DIR}/Select.cc)
    list(APPEND SRC_HEADERS ${SRC_DIR}/Select.h)
endif()

add_library (angel STATIC ${SRC_FILES})

install(TARGETS angel
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(FILES ${SRC_HEADERS} DESTINATION include/Angel)
