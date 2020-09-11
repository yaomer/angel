#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "http-context.h"

// [method url version\r\n]
void http_context::parse_line(const char *p, const char *ep)
{
    const char *next = std::find(p, ep, ' ');
    request.set_method(p, next);
    p = next + 1;
    next = std::find(p, ep, '?');
    if (next == ep) {
        next = std::find(p, ep, ' ');
        request.set_url(p, next);
    } else {
        request.set_url(p, next);
        while (true) {
            p = next + 1;
            next = std::find(p, ep, '&');
            if (next != ep)
                request.add_arg(p, next);
            else {
                next = std::find(p, ep, ' ');
                request.add_arg(p, next);
                break;
            }
        }
    }
    request.url().assign("." + request.url());
    p = next + 1;
    next = std::find(p, ep, '\r');
    request.set_version(p, next);
    state = PARSE_HEADER;
}

void http_context::parse_header(const char *p, const char *ep)
{
    if (strncmp(p, "\r\n", 2) == 0) {
        state = PARSE_LINE;
        return;
    }
    const char *next = std::find(p, ep, '\r');
    if (strncmp(p, "Host: ", 6) == 0) {
        request.set_host(p + 6, next);
    } else if (strncmp(p, "Connection: ", 12) == 0) {
        request.set_connection(p + 12, next);
    } else
        ; // TODO: other options
}

void http_context::send_response(const angel::connection_ptr& conn)
{
    std::string buffer;
    response.set_server("angel");
    if (strncasecmp(request.connection().c_str(), "Keep-Alive", 10) == 0)
        is_keepalive = true;
    else if (strncasecmp(request.connection().c_str(), "Close", 5) == 0)
        is_keepalive = false;
    if (request.method() == "GET") {
        if (request.url() == "./")
            request.url().append("index.html");
        fcat(buffer);
        response.set_body(buffer);
    } else {
        response.set_status_code(http_response::UNIMPLEMENTED);
        response.set_status_message("Method Unimplemented");
    }
    response.fill_response();
    conn->send(response.response());
    response.clear();
    if (!is_keepalive) {
        conn->close();
    }
}

void http_context::fcat(std::string& buffer)
{
    int fd = open(request.url().c_str(), O_RDONLY);
    if (fd > 0) {
        angel::buffer buf;
        while (buf.read_fd(fd) > 0)
            buffer.append(buf.peek(), buf.readable());
        response.set_status_code(http_response::OK);
        response.set_status_message("OK");
        close(fd);
    } else {
        response.set_status_code(http_response::NOT_FOUND);
        response.set_status_message("Not Found");
    }
}
