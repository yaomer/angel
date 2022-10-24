#ifndef __ANGEL_HTTPLIB_H
#define __ANGEL_HTTPLIB_H

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <future>

#include <angel/server.h>
#include <angel/client.h>
#include <angel/evloop_thread.h>
#include <angel/resolver.h>
#include <angel/util.h>

namespace angel {
namespace httplib {

enum Version {
    HTTP_VERSION_1_0, // HTTP/1.0
    HTTP_VERSION_1_1, // HTTP/1.1
};

enum Method {
    OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE, CONNECT,
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

enum ParseState {
    ParseLine,
    ParseHeader,
    ParseBody,
};

// HTTP Message base class
struct message {
    Headers headers;
    std::string body;
    size_t length;
    bool chunked = false;
    ssize_t chunk_size = -1;
    StatusCode parse_header(buffer& buf);
    StatusCode parse_body_length();
    StatusCode parse_body(buffer& buf);
    StatusCode parse_body_by_content_length(buffer& buf);
    StatusCode parse_body_by_chunked(buffer& buf);
    void clear();
};

struct uri {
    // Encode characters other than "-_.~", letters and numbers.
    static std::string encode(std::string_view uri);
    static bool decode(std::string_view uri, std::string& res);
    static bool parse_params(const char *first, const char *last, Params& params);

    bool parse(std::string_view uri);
    void clear();
    std::string scheme;
    std::string host;
    int port;
    std::string path;
    Params params;
    std::string fragment;
};

//====================================================
//=================== HttpServer =====================
//====================================================

class request : private message {
public:
    Method method() const { return req_method; }
    const std::string& path() const { return abs_path; }
    Version version() const { return http_version; }
    const Params& params() const { return query_params; }
    const Headers& headers() const { return message::headers; }
    const std::string& body() const { return message::body; }
private:
    StatusCode parse_line(buffer& buf);
    void clear();

    ParseState state = ParseLine;

    Method req_method;
    std::string abs_path;
    Version http_version;
    Params query_params;
    bool has_file;
    off_t filesize;
    std::string last_modified;
    std::string etag;
    friend class HttpServer;
};

class response {
public:
    response() = default;
    void set_status_code(StatusCode code);
    void add_header(std::string_view field, std::string_view value);
    void set_content(std::string_view body, std::string_view type = "text/plain");
    void send_chunk(std::string_view chunk);
    void send_done();
private:
    std::string& header();

    void append_status_line();

    void send(std::string_view body = "");
    void send_err();

    connection *conn;
    StatusCode status_code;
    Headers headers;
    std::string buf;
    bool chunked = false;
    std::string chunked_buf;
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
typedef std::function<void(request&, Headers&)> FileHandler;

class HttpServer {
public:
    HttpServer(evloop *, inet_addr);
    HttpServer& Get(std::string_view path, const ServerHandler handler);
    HttpServer& Post(std::string_view path, const ServerHandler handler);
    HttpServer& File(std::string_view path, const FileHandler handler);
    // For static file
    void set_base_dir(std::string_view dir);
    // Set parallel threads for request
    void set_parallel(unsigned n);
    // Set idle time for http connection
    void set_idle(int secs);
    // Set how to generate file etag
    // 1) default: file mtime "-" file size
    // 2) sha1: file sha1 digest "-" file size
    void generate_file_etag_by(std::string_view way);
    void start();
private:
    void message_handler(const connection_ptr&, buffer&);
    void process_request(const connection_ptr&, request& req, response& res);

    bool handle_user_router(request& req, response& res);
    void handle_file_router(request& req, response& res);
    void handle_static_file_request(request& req, response& res);
    void handle_range_request(request& req, response& res);

    bool keepalive(request& req);

    ConditionCode handle_conditional(request& req, response& res);
    ConditionCode if_match(request& req, response& res);
    ConditionCode if_modified_since(request& req, response& res);
    ConditionCode if_none_match(request& req, response& res);
    ConditionCode if_range(request& req, response& res);
    ConditionCode if_unmodified_since(request& req, response& res);

    ConditionCode expect(request& req, response& res);

    void send_file(request& req, response& res);
    void update_file(request& req, response& res);
    void delete_file(request& req, response& res);

    angel::server server;
    typedef std::unordered_map<std::string, ServerHandler> Table;
    std::unordered_map<Method, Table> router;
    std::unordered_map<std::string, FileHandler> file_table;
    std::string base_dir;
    int idle_time;
    bool generate_file_etag_by_sha1 = false;
};

//====================================================
//=================== HttpClient =====================
//====================================================

class http_request {
public:
    // Convenient chain call
    http_request& set_url(std::string_view url);
    http_request& set_params(const Params& params);
    http_request& set_headers(const Headers& headers);
    http_request& set_body(std::string_view body);
    http_request& set_resolve_timeout(int ms);
    http_request& set_connection_timeout(int ms);
    http_request& set_request_timeout(int ms);
    http_request& Get();
    http_request& Post();
private:
    std::string& str();
    uri uri;
    bool invalid_url = true;
    Headers headers;
    std::string body;
    std::string buf;
    int resolve_timeout = 1000 * 10;
    int connection_timeout = 1000 * 10;
    int request_timeout = 1000 * 10;
    std::string method;
    friend class HttpClient;
};

class http_response : private message {
public:
    std::string err;
    Version http_version;
    int status_code;
    std::string status_message;
    const std::string& body() { return message::body; }
private:
    StatusCode parse_line(buffer&);

    ParseState state = ParseLine;

    friend class HttpClient;
};

typedef std::future<http_response> response_future;

struct http_connection_pool;

struct http_connection {
    http_connection_pool *pool;
    std::unique_ptr<angel::client> client;
    std::promise<http_response> response_promise;
    http_response response;
    bool create; // The newly created connection
    bool leased; // Indicates the connection state
};

struct http_connection_pool {
    std::vector<std::string> addrs;
    std::vector<std::unique_ptr<http_connection>> avail;
    std::vector<std::unique_ptr<http_connection>> leased;
    bool pending; // Have a request pending ?
    std::mutex pending_mutex;
    std::condition_variable pending_cv;
    // Unlocked
    void lease_connection();
    void release_connection(http_connection*);
    http_connection *create_connection();
    void remove_connection(http_connection*);
    void wakeup();
    int active_conns();
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    void set_max_conns_per_route(int conns);
    response_future send(http_request&);
private:
    http_connection_pool *create_connection_pool(http_request&);
    void add_connection(http_connection_pool*, http_request&);
    void remove_connection(http_connection *);
    std::pair<http_connection_pool*, http_connection*> get_connection(http_request&);
    void put_connection(http_connection *);

    void receive(http_connection*, buffer&);

    evloop_thread sender;
    dns::resolver *resolver;
    // <host:port, pool>
    std::unordered_map<std::string, std::unique_ptr<http_connection_pool>> router;
    std::mutex router_mutex;
    int max_conns_per_route = 10;
};

}
}

#endif // __ANGEL_HTTPLIB_H
