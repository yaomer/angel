#ifndef __ANGEL_HTTPLIB_H
#define __ANGEL_HTTPLIB_H

#include <angel/server.h>

#include <vector>
#include <string>
#include <unordered_map>

namespace angel {
namespace httplib {

enum StatusCode {
    // Informational 1xx
    Continue = 100,
    SwitchingProtocols = 101,
    // Successful 2xx
    Ok = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritativeInformation = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,
    // Redirection 3xx
    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    UseProxy = 305,
    TemporaryRedirect = 307,
    // Client Error 4xx
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout = 408,
    Conflict = 409,
    Gone = 410,
    LengthRequired = 411,
    PreconditionFailed = 412,
    RequestEntityTooLarge = 413,
    RequestUriTooLong = 414,
    UnsupportedMediaType = 415,
    RequestedRangeNotSatisfiable = 416,
    ExpectationFailed = 417,
    // Server Error 5xx
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HttpVersionNotSupported = 505,
};

enum ParseState {
    ParseLine,
    ParseHeader,
    ParseBody,
};

struct request {
    std::string method;
    std::string path;
    std::string version;
    std::string host;
    std::string connection;
    std::vector<std::string> args;
private:
    bool parse_line(buffer& buf, int crlf);
    bool parse_header(buffer& buf, int crlf);
    friend class HttpServer;
};

class response {
public:
    response() = default;
    void set_status_code(int code)
    { status_code = code; }
    void set_status_message(std::string_view message)
    { status_message.assign(message); }
    void add_header(std::string_view field, std::string_view value)
    { headers.emplace(field, value); }
    void set_content(std::string_view data)
    { content.assign(data); }
private:
    std::string& str();
    void clear();

    int status_code;
    std::string status_message;
    std::unordered_map<std::string, std::string> headers;
    std::string content;
    std::string buf;
    friend class HttpServer;
};

struct uri {
    // Encode characters other than "-_.~", letters and numbers.
    static std::string encode(std::string_view uri);
    static std::string decode(std::string_view uri);
};

struct context {
    request request;
    response response;
    int state = ParseLine;
};

typedef std::function<void(request&, response&)> ServerHandler;

class HttpServer {
public:
    HttpServer(evloop *, inet_addr);
    HttpServer& Get(std::string_view path, const ServerHandler handler);
    // For static file
    void set_base_dir(std::string_view dir);
    void set_parallel(unsigned n);
    void start();
private:
    void message_handler(const connection_ptr&, buffer&);
    void process_request(const connection_ptr&);

    angel::server server;
    typedef std::unordered_map<std::string, ServerHandler> Table;
    std::unordered_map<std::string, Table> router;
    std::string base_dir;
};

}
}

#endif // __ANGEL_HTTPLIB_H
