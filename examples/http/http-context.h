#ifndef _ANGEL_HTTP_CONTEXT_H
#define _ANGEL_HTTP_CONTEXT_H

#include <angel/connection.h>

#include <string>

#include "http-request.h"
#include "http-response.h"

class http_context {
public:
    enum {
        PARSE_LINE,
        PARSE_HEADER,
    };
    http_context() : state(PARSE_LINE), is_keepalive(false)
    {
    }
    int get_state() const { return state; }
    void set_state(int s) { state = s; }
    void parse_line(const char *p, const char *ep);
    void parse_header(const char *p, const char *ep);
    void send_response(const angel::connection_ptr& conn);
private:
    void fcat(std::string& buffer);

    int state;
    bool is_keepalive;
    http_request request;
    http_response response;
};

#endif // _ANGEL_HTTP_CONTEXT_H
