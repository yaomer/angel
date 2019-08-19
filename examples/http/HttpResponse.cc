#include <iostream>
#include <sys/time.h>
#include "HttpResponse.h"

void HttpResponse::fillResponse()
{
    char buf[64];

    snprintf(buf, sizeof(buf), "HTTP/1.0 %d ", _statusCode);
    _buffer.append(buf);
    _buffer.append(_statusMessage);
    _buffer.append("\r\n");

    if (!_body.empty()) {
        snprintf(buf, sizeof(buf), "%zu", _body.size());
        setContentLength(buf);
    }

    struct tm tm;
    time_t now = time(nullptr);
    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%a, %b %d, %Y %X-%Z", &tm);
    _headers["Date"] = buf;

    for (auto& it : _headers) {
        _buffer.append(it.first);
        _buffer.append(": ");
        _buffer.append(it.second);
        _buffer.append("\r\n");
    }
    _buffer.append("\r\n");
    if (!_body.empty())
        _buffer.append(_body);
}

void HttpResponse::clear()
{
    _body.clear();
    _headers.clear();
    _buffer.clear();
}
