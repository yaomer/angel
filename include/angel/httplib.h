#ifndef __ANGEL_HTTPLIB_H
#define __ANGEL_HTTPLIB_H

#include <vector>
#include <string>
#include <unordered_map>

#include <angel/server.h>
#include <angel/util.h>

namespace angel {
namespace httplib {

enum Method {
    OPTIONS,
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    TRACE,
    CONNECT,
};

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

const char *to_str(StatusCode code);

enum ParseState {
    ParseLine,
    ParseHeader,
    ParseBody,
};

struct field {
    field(const char *s) : val(s) {  }
    field(std::string_view s) : val(s) {  }
    std::string val;
};

inline bool operator==(const field& f1, const field& f2)
{
    return util::equal_case(f1.val, f2.val);
}

struct field_hash {
    bool operator()(const field& f) const
    {
        return std::hash<std::string>()(util::to_lower(f.val));
    }
};

typedef std::unordered_map<std::string, std::string> Params;
typedef std::unordered_map<field, std::string, field_hash> Headers;

class request {
public:
    Method method() const { return req_method; }
    const std::string& path() const { return req_path; }
    const std::string& version() const { return req_version; }
    const std::string& body() const { return req_body; }
    const Params& params() const { return req_params; }
    const Headers& headers() const { return req_headers; }
private:
    StatusCode parse_line(buffer& buf);
    StatusCode parse_header(buffer& buf);
    StatusCode parse_body_length();
    StatusCode parse_body(buffer& buf);
    StatusCode parse_body_by_content_length(buffer& buf);
    StatusCode parse_body_by_chunked(buffer& buf);
    void clear();

    int state = ParseLine;

    Method req_method;
    std::string req_path;
    std::string req_version;
    std::string req_body;
    size_t length;
    bool chunked = false;
    ssize_t chunk_size = -1;
    off_t filesize;
    std::string last_modified;
    std::string etag;

    Params req_params;
    Headers req_headers;
    friend class HttpServer;
};

class response {
public:
    response() = default;
    void set_status_code(StatusCode code);
    void add_header(std::string_view field, std::string_view value);
    // default type = "text/plain"
    void set_content(std::string_view data);
    void set_content(std::string_view data, std::string_view type);
private:
    std::string& str();

    void append_status_line();

    int status_code;
    std::string status_message;
    Headers headers;
    std::string content;
    std::string buf;
    friend class HttpServer;
    friend struct byte_range_set;
};

struct context {
    request request;
    response response;
};

enum ConditionCode {
    NoHeader,
    Successful,
    Failed,
};

typedef std::function<void(request&, response&)> ServerHandler;

class HttpServer {
public:
    HttpServer(evloop *, inet_addr);
    HttpServer& Get(std::string_view path, const ServerHandler handler);
    HttpServer& Post(std::string_view path, const ServerHandler handler);
    // For static file
    void set_base_dir(std::string_view dir);
    void set_parallel(unsigned n);
    void start();
private:
    void message_handler(const connection_ptr&, buffer&);
    void process_request(const connection_ptr&, request& req, response& res);

    bool handle_register_request(request& req, response& res);
    bool handle_static_file_request(const connection_ptr& conn, request& req, response& res);
    void handle_range_request(const connection_ptr& conn, request& req, response& res);

    bool keepalive(request& req);

    ConditionCode handle_conditional(const connection_ptr& conn, request& req, response& res);
    ConditionCode if_match(const connection_ptr& conn, request& req, response& res);
    ConditionCode if_modified_since(const connection_ptr& conn, request& req, response& res);
    ConditionCode if_none_match(const connection_ptr& conn, request& req, response& res);
    ConditionCode if_range(const connection_ptr& conn, request& req, response& res);
    ConditionCode if_unmodified_since(const connection_ptr& conn, request& req, response& res);

    ConditionCode expect(const connection_ptr& conn, request& req, response& res);

    void send_file(const connection_ptr& conn, request& req, response& res);

    angel::server server;
    typedef std::unordered_map<std::string, ServerHandler> Table;
    std::unordered_map<Method, Table> router;
    std::string base_dir;
};

}
}

#endif // __ANGEL_HTTPLIB_H
