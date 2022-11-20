cmake_minimum_required (VERSION 3.1)

project (angel)

set (CMAKE_CXX_FLAGS "-std=c++17 -Wall -O2")

include (CheckFunctionExists)

CHECK_FUNCTION_EXISTS (poll ANGEL_HAVE_POLL)
CHECK_FUNCTION_EXISTS (epoll_wait ANGEL_HAVE_EPOLL)
CHECK_FUNCTION_EXISTS (kqueue ANGEL_HAVE_KQUEUE)
CHECK_FUNCTION_EXISTS (select ANGEL_HAVE_SELECT)

option (ANGEL_USE_OPENSSL "Build angel with SSL" OFF)

if (ANGEL_USE_OPENSSL)
    find_package (OpenSSL)
endif()

set (SRC_DIR "${PROJECT_SOURCE_DIR}/src")

include_directories (${PROJECT_SOURCE_DIR}/include)

configure_file (
    "${SRC_DIR}/config.h.in"
    "${PROJECT_SOURCE_DIR}/include/angel/config.h"
)

set (SRC_FILES 
    ${SRC_DIR}/evloop.cc
    ${SRC_DIR}/channel.cc
    ${SRC_DIR}/connection.cc
    ${SRC_DIR}/inet_addr.cc
    ${SRC_DIR}/sockops.cc
    ${SRC_DIR}/listener.cc
    ${SRC_DIR}/connector.cc
    ${SRC_DIR}/server.cc
    ${SRC_DIR}/client.cc
    ${SRC_DIR}/timer.cc
    ${SRC_DIR}/signal.cc
    ${SRC_DIR}/buffer.cc
    ${SRC_DIR}/logger.cc
    ${SRC_DIR}/util.cc
    ${SRC_DIR}/sha1.cc
    ${SRC_DIR}/base64.cc
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

if (ANGEL_USE_OPENSSL)
    list(APPEND SRC_FILES
        ${SRC_DIR}/ssl/ssl_handshake.cc
        ${SRC_DIR}/ssl/ssl_filter.cc
        ${SRC_DIR}/ssl/ssl_connection.cc
        ${SRC_DIR}/ssl/ssl_client.cc
        ${SRC_DIR}/ssl/ssl_server.cc
    )
endif()

list(APPEND SRC_FILES ${SRC_DIR}/dns/resolver.cc)

list(APPEND SRC_FILES ${SRC_DIR}/websocket/ws-server.cc)

list(APPEND SRC_FILES
    ${SRC_DIR}/httplib/httplib.cc
    ${SRC_DIR}/httplib/util.cc
)

list(APPEND SRC_FILES ${SRC_DIR}/smtplib/smtplib.cc)

list(APPEND SRC_FILES
    ${SRC_DIR}/mime/mime.cc
    ${SRC_DIR}/mime/charset.cc
    ${SRC_DIR}/mime/codec.cc
    ${SRC_DIR}/mime/qp.cc
    ${SRC_DIR}/mime/mime_types.cc
)

add_library (angel STATIC ${SRC_FILES})

if (ANGEL_USE_OPENSSL)
    target_link_libraries(angel ssl crypto)
endif()

install(TARGETS angel
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/angel DESTINATION include)

set (TEST "${PROJECT_SOURCE_DIR}/test")
set (SAMPLE "${PROJECT_SOURCE_DIR}/sample")

function(add_test target src)
    add_executable(${target} ${TEST}/${src})
    target_link_libraries(${target} angel)
endfunction()

function(add_sample target src)
    add_executable(${target} ${SAMPLE}/${src})
    target_link_libraries(${target} angel)
endfunction()

add_test(bench bench.cc)

add_test(bench_http bench_http.cc)

add_sample(echo-server echo-server.cc)
add_sample(echo-client echo-client.cc)

add_sample(echo-channel echo-channel.cc)

add_sample(chargen-server chargen-server.cc)
add_sample(chargen-client chargen-client.cc)

add_sample(discard-server discard-server.cc)
add_sample(discard-client discard-client.cc)

add_sample(http-server http-server.cc)
add_sample(http-client http-client.cc)

add_sample(dns-resolve dns-resolve.cc)

add_sample(time-server time-server.cc)
add_sample(time-client time-client.cc)

add_sample(daytime-server daytime-server.cc)

add_sample(chat-server chat-server.cc)

add_sample(fib-server fib-server.cc)

add_sample(signal-test signal-test.cc)
