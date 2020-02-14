#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "HttpContext.h"

// [method url version\r\n]
void HttpContext::parseLine(const char *p, const char *ep)
{
    const char *next = std::find(p, ep, ' ');
    _request.setMethod(p, next);
    p = next + 1;
    next = std::find(p, ep, '?');
    if (next == ep) {
        next = std::find(p, ep, ' ');
        _request.setUrl(p, next);
    } else {
        _request.setUrl(p, next);
        while (true) {
            p = next + 1;
            next = std::find(p, ep, '&');
            if (next != ep)
                _request.addArg(p, next);
            else {
                next = std::find(p, ep, ' ');
                _request.addArg(p, next);
                break;
            }
        }
    }
    _request.url().assign("." + _request.url());
    p = next + 1;
    next = std::find(p, ep, '\r');
    _request.setVersion(p, next);
    _state = PARSE_HEADER;
}

void HttpContext::parseHeader(const char *p, const char *ep)
{
    if (strncmp(p, "\r\n", 2) == 0) {
        _state = PARSE_LINE;
        return;
    }
    const char *next = std::find(p, ep, '\r');
    if (strncmp(p, "Host: ", 6) == 0) {
        _request.setHost(p + 6, next);
    } else if (strncmp(p, "Connection: ", 12) == 0) {
        _request.setConnection(p + 12, next);
    } else
        ;
}

void HttpContext::response(const Angel::TcpConnectionPtr& conn)
{
    std::string buffer;
    _response.setServer("Angel");
    if (strncasecmp(_request.connection().c_str(), "Keep-Alive", 10) == 0)
        _keepAlive = true;
    else if (strncasecmp(_request.connection().c_str(), "Close", 5) == 0)
        _keepAlive = false;
    if (_request.method().compare("GET") == 0) {
        if (_request.url().compare("./") == 0)
            _request.url().append("index.html");
        fcat(buffer);
        _response.setBody(buffer);
    } else {
        _response.setStatusCode(HttpResponse::UNIMPLEMENTED);
        _response.setStatusMessage("Method Unimplemented");
    }
    _response.fillResponse();
    conn->send(_response.buffer());
    _response.clear();
    if (!_keepAlive) {
        conn->close();
    }
}

void HttpContext::fcat(std::string& buffer)
{
    int fd = open(_request.url().c_str(), O_RDONLY);
    if (fd > 0) {
        Angel::Buffer buf;
        while (buf.readFd(fd) > 0)
            buffer.append(buf.peek(), buf.readable());
        _response.setStatusCode(HttpResponse::OK);
        _response.setStatusMessage("OK");
        close(fd);
    } else {
        _response.setStatusCode(HttpResponse::NOT_FOUND);
        _response.setStatusMessage("Not Found");
    }
}
