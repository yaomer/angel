cmake_minimum_required (VERSION 3.1)

project (Angel)

set (CMAKE_CXX_FLAGS "-std=c++17 -Wall -O2")

include (CheckFunctionExists)
include (CheckSymbolExists)

CHECK_FUNCTION_EXISTS (poll _ANGEL_HAVE_POLL)
CHECK_FUNCTION_EXISTS (epoll_wait _ANGEL_HAVE_EPOLL)
CHECK_FUNCTION_EXISTS (kqueue _ANGEL_HAVE_KQUEUE)
CHECK_FUNCTION_EXISTS (select _ANGEL_HAVE_SELECT)

CHECK_SYMBOL_EXISTS (TCP_KEEPALIVE netinet/tcp.h _ANGEL_HAVE_TCP_KEEPALIVE)
CHECK_SYMBOL_EXISTS (SO_KEEPALIVE netinet/tcp.h _ANGEL_HAVE_SO_KEEPALIVE)

set (PROJECT_SRC_DIR "${PROJECT_SOURCE_DIR}/src")

configure_file (
    "${PROJECT_SRC_DIR}/config.h.in"
    "${PROJECT_SRC_DIR}/config.h"
)

set (SRC_HEADERS
    ${PROJECT_SRC_DIR}/Acceptor.h
    ${PROJECT_SRC_DIR}/Connector.h
    ${PROJECT_SRC_DIR}/Buffer.h
    ${PROJECT_SRC_DIR}/Channel.h
    ${PROJECT_SRC_DIR}/EventLoop.h
    ${PROJECT_SRC_DIR}/EventLoopThread.h
    ${PROJECT_SRC_DIR}/EventLoopThreadPool.h
    ${PROJECT_SRC_DIR}/TcpConnection.h
    ${PROJECT_SRC_DIR}/InetAddr.h
    ${PROJECT_SRC_DIR}/Socket.h
    ${PROJECT_SRC_DIR}/SockOps.h
    ${PROJECT_SRC_DIR}/TcpServer.h
    ${PROJECT_SRC_DIR}/TcpClient.h
    ${PROJECT_SRC_DIR}/Timer.h
    ${PROJECT_SRC_DIR}/Signaler.h
    ${PROJECT_SRC_DIR}/ThreadPool.h
    ${PROJECT_SRC_DIR}/LogStream.h
    ${PROJECT_SRC_DIR}/Logger.h
    ${PROJECT_SRC_DIR}/Poller.h
    ${PROJECT_SRC_DIR}/noncopyable.h
    ${PROJECT_SRC_DIR}/decls.h
    ${PROJECT_SRC_DIR}/util.h
)

set (SRC_FILES 
    ${PROJECT_SRC_DIR}/EventLoop.cc
    ${PROJECT_SRC_DIR}/EventLoopThread.cc
    ${PROJECT_SRC_DIR}/EventLoopThreadPool.cc
    ${PROJECT_SRC_DIR}/Channel.cc
    ${PROJECT_SRC_DIR}/Buffer.cc
    ${PROJECT_SRC_DIR}/TcpConnection.cc
    ${PROJECT_SRC_DIR}/InetAddr.cc
    ${PROJECT_SRC_DIR}/SockOps.cc
    ${PROJECT_SRC_DIR}/Acceptor.cc
    ${PROJECT_SRC_DIR}/Connector.cc
    ${PROJECT_SRC_DIR}/TcpServer.cc
    ${PROJECT_SRC_DIR}/TcpClient.cc
    ${PROJECT_SRC_DIR}/Timer.cc
    ${PROJECT_SRC_DIR}/Signaler.cc
    ${PROJECT_SRC_DIR}/ThreadPool.cc
    ${PROJECT_SRC_DIR}/LogStream.cc
    ${PROJECT_SRC_DIR}/Logger.cc
    ${PROJECT_SRC_DIR}/util.cc
)

if (_ANGEL_HAVE_POLL)
    list(APPEND SRC_FILES ${PROJECT_SRC_DIR}/Poll.cc)
endif()

if (_ANGEL_HAVE_EPOLL)
    list(APPEND SRC_FILES ${PROJECT_SRC_DIR}/Epoll.cc)
endif()

if (_ANGEL_HAVE_KQUEUE)
    list(APPEND SRC_FILES ${PROJECT_SRC_DIR}/Kqueue.cc)
endif()

if (_ANGEL_HAVE_SELECT)
    list(APPEND SRC_FILES ${PROJECT_SRC_DIR}/Select.cc)
endif()

add_library (angel STATIC ${SRC_FILES})

install(TARGETS angel
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(FILES ${SRC_HEADERS} DESTINATION include/Angel)
