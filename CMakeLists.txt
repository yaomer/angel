cmake_minimum_required (VERSION 3.1)

project (angel)

set (CMAKE_CXX_FLAGS "-std=c++17 -Wall -O2")

include (CheckFunctionExists)
include (CheckSymbolExists)

CHECK_FUNCTION_EXISTS (poll ANGEL_HAVE_POLL)
CHECK_FUNCTION_EXISTS (epoll_wait ANGEL_HAVE_EPOLL)
CHECK_FUNCTION_EXISTS (kqueue ANGEL_HAVE_KQUEUE)
CHECK_FUNCTION_EXISTS (select ANGEL_HAVE_SELECT)

CHECK_SYMBOL_EXISTS (TCP_KEEPALIVE netinet/tcp.h ANGEL_HAVE_TCP_KEEPALIVE)
CHECK_SYMBOL_EXISTS (SO_KEEPALIVE netinet/tcp.h ANGEL_HAVE_SO_KEEPALIVE)

set (SRC_DIR "${PROJECT_SOURCE_DIR}/src")

configure_file (
    "${SRC_DIR}/config.h.in"
    "${SRC_DIR}/config.h"
)

set (SRC_HEADERS
    ${SRC_DIR}/listener.h
    ${SRC_DIR}/connector.h
    ${SRC_DIR}/buffer.h
    ${SRC_DIR}/channel.h
    ${SRC_DIR}/evloop.h
    ${SRC_DIR}/evloop_thread.h
    ${SRC_DIR}/evloop_thread_pool.h
    ${SRC_DIR}/connection.h
    ${SRC_DIR}/inet_addr.h
    ${SRC_DIR}/socket.h
    ${SRC_DIR}/sockops.h
    ${SRC_DIR}/server.h
    ${SRC_DIR}/client.h
    ${SRC_DIR}/timer.h
    ${SRC_DIR}/signaler.h
    ${SRC_DIR}/thread_pool.h
    ${SRC_DIR}/logger.h
    ${SRC_DIR}/poller.h
    ${SRC_DIR}/util.h
    ${SRC_DIR}/event.h
    ${SRC_DIR}/noncopyable.h
    ${SRC_DIR}/enum_bitwise.h
)

set (SRC_FILES 
    ${SRC_DIR}/evloop.cc
    ${SRC_DIR}/channel.cc
    ${SRC_DIR}/connection.cc
    ${SRC_DIR}/sockops.cc
    ${SRC_DIR}/listener.cc
    ${SRC_DIR}/connector.cc
    ${SRC_DIR}/server.cc
    ${SRC_DIR}/client.cc
    ${SRC_DIR}/timer.cc
    ${SRC_DIR}/signaler.cc
    ${SRC_DIR}/buffer.cc
    ${SRC_DIR}/logger.cc
    ${SRC_DIR}/util.cc
)

if (ANGEL_HAVE_POLL)
    list(APPEND SRC_FILES ${SRC_DIR}/poll.cc)
endif()

if (ANGEL_HAVE_EPOLL)
    list(APPEND SRC_FILES ${SRC_DIR}/epoll.cc)
endif()

if (ANGEL_HAVE_KQUEUE)
    list(APPEND SRC_FILES ${SRC_DIR}/kqueue.cc)
endif()

if (ANGEL_HAVE_SELECT)
    list(APPEND SRC_FILES ${SRC_DIR}/select.cc)
endif()

add_library (angel STATIC ${SRC_FILES})

install(TARGETS angel
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(FILES ${SRC_HEADERS} DESTINATION include/angel)
