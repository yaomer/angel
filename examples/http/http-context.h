#ifndef _ANGEL_HTTP_CONTEXT_H
#define _ANGEL_HTTP_CONTEXT_H

#include <angel/connection.h>

#include <vector>
#include <string>
#include <map>

struct http_request {
    std::string method;
    std::string url;
    std::string version;
    std::string host;
    std::string connection;
    std::vector<std::string> arglist;
};

class http_response {
public:
    enum StatusCode {
        Ok = 200,
        NotFound = 404,
        Unimplemented = 501,
    };
    std::string& response() { return buffer; }
    void set_status_code(int code)
    { status_code = code; }
    void set_status_message(const std::string& message)
    { status_message.assign(message); }
    void set_server(const std::string& server)
    { headers["Server"] = server; }
    void set_content_type(const std::string& type)
    { headers["Content-Type"] = type; }
    void set_content_length(const std::string& length)
    { headers["Content-Length"] = length; }
    void set_date(const std::string& date)
    { headers["Date"] = date; }
    void set_body(const std::string& data)
    { body.assign(data); }
    void fill();
    void clear();
private:
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string buffer;
};

class http_context {
public:
    enum ParseState {
        Line,
        Header,
    };
    http_context() : state(Line), is_keepalive(false)
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
