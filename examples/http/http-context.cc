#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "http-context.h"

// [method url version\r\n]
void http_context::parse_line(const char *p, const char *ep)
{
    const char *next = std::find(p, ep, ' ');
    request.method.assign(p, next);
    p = next + 1;
    next = std::find(p, ep, '?');
    if (next == ep) {
        next = std::find(p, ep, ' ');
        request.url.assign(p, next);
    } else {
        request.url.assign(p, next);
        while (true) {
            p = next + 1;
            next = std::find(p, ep, '&');
            if (next != ep)
                request.arglist.emplace_back(p, next);
            else {
                next = std::find(p, ep, ' ');
                request.arglist.emplace_back(p, next);
                break;
            }
        }
    }
    request.url.assign("." + request.url);
    p = next + 1;
    next = std::find(p, ep, '\r');
    request.version.assign(p, next);
    state = Header;
}

void http_context::parse_header(const char *p, const char *ep)
{
    if (strncmp(p, "\r\n", 2) == 0) {
        state = Line;
        return;
    }
    const char *next = std::find(p, ep, '\r');
    if (strncmp(p, "Host: ", 6) == 0) {
        request.host.assign(p + 6, next);
    } else if (strncmp(p, "Connection: ", 12) == 0) {
        request.connection.assign(p + 12, next);
    } else
        ; // TODO: other options
}

void http_response::fill()
{
    char buf[64];

    snprintf(buf, sizeof(buf), "HTTP/1.0 %d ", status_code);
    buffer.append(buf)
          .append(status_message)
          .append("\r\n");

    if (!body.empty()) {
        set_content_length(std::to_string(body.size()));
    }

    struct tm tm;
    time_t now = time(nullptr);
    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%a, %b %d, %Y %X-%Z", &tm);
    set_date(buf);

    for (auto& it : headers) {
        buffer.append(it.first)
              .append(": ")
              .append(it.second)
              .append("\r\n");
    }
    buffer.append("\r\n");

    if (!body.empty())
        buffer.append(body);
}

void http_response::clear()
{
    body.clear();
    headers.clear();
    buffer.clear();
}

void http_context::send_response(const angel::connection_ptr& conn)
{
    std::string buffer;
    response.set_server("angel");
    if (strncasecmp(request.connection.c_str(), "Keep-Alive", 10) == 0)
        is_keepalive = true;
    else if (strncasecmp(request.connection.c_str(), "Close", 5) == 0)
        is_keepalive = false;
    if (request.method == "GET") {
        if (request.url == "./")
            request.url.append("index.html");
        fcat(buffer);
        response.set_body(buffer);
    } else {
        response.set_status_code(http_response::Unimplemented);
        response.set_status_message("Method Unimplemented");
    }
    response.fill();
    conn->send(response.response());
    response.clear();
    if (!is_keepalive) {
        conn->close();
    }
}

void http_context::fcat(std::string& buffer)
{
    int fd = open(request.url.c_str(), O_RDONLY);
    if (fd > 0) {
        angel::buffer buf;
        while (buf.read_fd(fd) > 0)
            buffer.append(buf.peek(), buf.readable());
        response.set_status_code(http_response::Ok);
        response.set_status_message("OK");
        close(fd);
    } else {
        response.set_status_code(http_response::NotFound);
        response.set_status_message("Not Found");
    }
}
