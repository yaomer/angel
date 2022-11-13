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
    ${SRC_DIR}/signaler.cc
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

add_executable(bench ${TEST}/bench.cc)
target_link_libraries(bench angel)

set (SAMPLE "${PROJECT_SOURCE_DIR}/sample")

add_executable(echo-server ${SAMPLE}/echo-server.cc)
target_link_libraries(echo-server angel)
add_executable(echo-client ${SAMPLE}/echo-client.cc)
target_link_libraries(echo-client angel)

add_executable(chargen-server ${SAMPLE}/chargen-server.cc)
target_link_libraries(chargen-server angel)
add_executable(chargen-client ${SAMPLE}/chargen-client.cc)
target_link_libraries(chargen-client angel)

add_executable(discard-server ${SAMPLE}/discard-server.cc)
target_link_libraries(discard-server angel)
add_executable(discard-client ${SAMPLE}/discard-client.cc)
target_link_libraries(discard-client angel)

add_executable(time-server ${SAMPLE}/time-server.cc)
target_link_libraries(time-server angel)
add_executable(time-client ${SAMPLE}/time-client.cc)
target_link_libraries(time-client angel)

add_executable(daytime-server ${SAMPLE}/daytime-server.cc)
target_link_libraries(daytime-server angel)

add_executable(chat-server ${SAMPLE}/chat-server.cc)
target_link_libraries(chat-server angel)

add_executable(fib-server ${SAMPLE}/fib-server.cc)
target_link_libraries(fib-server angel)

add_executable(http-server ${SAMPLE}/http-server.cc)
target_link_libraries(http-server angel)

add_executable(signal-test ${SAMPLE}/signal-test.cc)
target_link_libraries(signal-test angel)

add_executable(dns-resolve ${SAMPLE}/dns-resolve.cc)
target_link_libraries(dns-resolve angel)
