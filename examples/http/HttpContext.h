#ifndef _ANGEL_HTTP_CONTEXT_H
#define _ANGEL_HTTP_CONTEXT_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include <Angel/TcpConnection.h>
#include <string>

class HttpContext {
public:
    enum {
        PARSE_LINE,
        PARSE_HEADER,
    };
    HttpContext() 
        : _state(PARSE_LINE), _keepAlive(false) {  }
    int state() const { return _state; }
    void setState(int state) { _state = state; }
    void parseReqLine(const char *p, const char *ep);
    void parseReqHeader(const char *p, const char *ep);
    void response(const Angel::TcpConnectionPtr& conn);
    void fcat(std::string& buffer);
private:
    int _state;
    bool _keepAlive;
    HttpRequest _request;
    HttpResponse _response;
};

#endif // _ANGEL_HTTP_CONTEXT_H
