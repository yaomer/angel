#include <angel/httplib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <chrono>
#include <iomanip>
#include <sstream>

#include <angel/mime.h>

namespace angel {
namespace httplib {

static mime::mimetypes mimetypes;

static const int uri_max_len = 1024 * 1024;

using Clock = std::chrono::system_clock;

// Date: Wed, 15 Nov 1995 06:25:24 GMT
static std::string format_date(const Clock::time_point& now)
{
    std::ostringstream oss;
    auto tm = Clock::to_time_t(now);
    oss << std::put_time(std::gmtime(&tm), "%a, %d %b %Y %T GMT");
    return oss.str();
}

static std::string format_date()
{
    return format_date(Clock::now());
}

static const std::unordered_map<std::string_view, Method> methods = {
    { "OPTIONS",    OPTIONS },
    { "GET",        GET },
    { "HEAD",       HEAD },
    { "POST",       POST },
    { "PUT",        PUT },
    { "DELETE",     DELETE },
    { "TRACE",      TRACE },
    { "CONNECT",    CONNECT },
};

// Request-Line = Method <SP> Request-URI <SP> HTTP-Version <CRLF>
StatusCode request::parse_line(buffer& buf, int crlf)
{
    const char *p = buf.peek();
    const char *linep = buf.peek() + crlf;

    // Parse Method
    const char *next = std::find(p, linep, ' ');
    if (next == linep) return BadRequest;

    std::string_view method(p, next - p);
    auto it = methods.find(method);
    if (it == methods.end()) return BadRequest;
    req_method = it->second;
    p = next + 1;

    // Parse Request-URI
    next = std::find(p, linep, ' ');
    if (next == linep) return BadRequest;
    const char *version = next + 1;

    auto decoded_uri = uri::decode(std::string_view(p, next - p));
    p = decoded_uri.data();
    const char *end = p + decoded_uri.size();

    // Have Parameters ?
    next = std::find(p, end, '?');
    req_path.assign(p, next);
    if (next != end) {
        p = next + 1;
        while (true) {
            const char *eq = std::find(p, end, '=');
            if (eq == end) return BadRequest;
            std::string_view key(p, eq - p);
            const char *sep = std::find(++eq, end, '&');
            std::string_view value(eq, sep - eq);
            req_params.emplace(key, value);
            if (sep == end) break;
            p = sep + 1;
        }
    }

    // Parse HTTP-Version
    req_version = util::to_upper(std::string_view(version, linep - version));
    if (req_version == "HTTP/1.0" || req_version == "HTTP/1.1") {
        // Nothing to do.
    } else if (req_version == "HTTP/2.0") {
        return HttpVersionNotSupported;
    } else {
        return BadRequest;
    }

    buf.retrieve(crlf + 2);
    return Ok;
}

// message-header = field-name ":" [ field-value ]
StatusCode request::parse_header(buffer& buf, int crlf)
{
    if (buf.starts_with("\r\n")) {
        buf.retrieve(crlf + 2);
        return Ok;
    }

    const char *p = buf.peek();
    const char *end = buf.peek() + crlf;

    int pos = buf.find(":");
    if (pos < 0) return BadRequest;

    std::string_view field(p, pos);
    p += pos + 1;
    while (p < end && isspace(*p)) p++;
    if (p == end) return BadRequest;
    std::string_view value(p, end - p);
    req_headers.emplace(field, value);

    buf.retrieve(crlf + 2);
    return Continue;
}

void request::parse_body(buffer& buf)
{
    long content_length = atol(req_headers["Content-Length"].c_str());
    req_body.assign(buf.peek(), content_length);
    buf.retrieve(content_length);
}

void response::set_status_code(StatusCode code)
{
    status_code = code;
    status_message = to_str(code);
}

void response::add_header(std::string_view field, std::string_view value)
{
    headers.emplace(field, value);
}

void response::set_content(std::string_view data)
{
    set_content(data, "text/plain");
}

void response::set_content(std::string_view data, std::string_view type)
{
    add_header("Content-Type", type);
    content.assign(data);
}

// Status-Line = HTTP-Version <SP> Status-Code <SP> Reason-Phrase <CRLF>
std::string& response::str()
{
    buf.clear();

    buf.append("HTTP/1.1 ")
       .append(std::to_string(status_code)).append(" ")
       .append(status_message).append("\r\n");

    if (!content.empty()) {
        add_header("Content-Length", std::to_string(content.size()));
    }
    add_header("Date", format_date());

    for (auto& [field, value] : headers) {
        buf.append(field.val).append(": ").append(value).append("\r\n");
    }
    buf.append("\r\n");
    buf.append(content);

    headers.clear();
    content.clear();

    return buf;
}

//==============================================
//================ HttpServer ==================
//==============================================

HttpServer::HttpServer(evloop *loop, inet_addr listen_addr)
    : server(loop, listen_addr)
{
    server.set_connection_handler([](const connection_ptr& conn){
            conn->set_context(context());
            });
    server.set_message_handler([this](const connection_ptr& conn, buffer& buf){
            this->message_handler(conn, buf);
            });
}

void HttpServer::set_base_dir(std::string_view dir)
{
    base_dir = dir;
}

void HttpServer::set_parallel(unsigned n)
{
    if (n == 0) return;
    server.start_io_threads(n);
}

void HttpServer::start()
{
    set_base_dir(".");
    server.start();
}

void HttpServer::message_handler(const connection_ptr& conn, buffer& buf)
{
    if (!conn->is_connected()) return;
    auto& ctx = std::any_cast<context&>(conn->get_context());
    printf("%s\n", buf.c_str());
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (ctx.state) {
        case ParseLine:
        {
            if (crlf > uri_max_len) {
                ctx.response.set_status_code(RequestUriTooLong);
                conn->send(ctx.response.str());
                conn->close();
                return;
            }
            StatusCode code = ctx.request.parse_line(buf, crlf);
            if (code != Ok) {
                ctx.response.set_status_code(code);
                conn->send(ctx.response.str());
                conn->close();
                return;
            }
            ctx.state = ParseHeader;
            break;
        }
        case ParseHeader:
        {
            StatusCode code = ctx.request.parse_header(buf, crlf);
            switch (code) {
            case Ok:
                if (ctx.request.method() == POST) {
                    if (!ctx.request.req_headers.count("Content-Length")) {
                        ctx.response.set_status_code(LengthRequired);
                        conn->send(ctx.response.str());
                        conn->close();
                        return;
                    }
                    ctx.request.parse_body(buf);
                }
                process_request(conn);
                break;
            case Continue:
                break;
            default:
                ctx.response.set_status_code(code);
                conn->send(ctx.response.str());
                conn->close();
                return;
            }
            break;
        }
        }
    }
}

void HttpServer::process_request(const connection_ptr& conn)
{
    auto& ctx = std::any_cast<context&>(conn->get_context());
    auto& req = ctx.request;
    auto& res = ctx.response;

    res.add_header("Server", "angel");

    switch (req.method()) {
    case GET:
    {
        auto& table = router[GET];
        auto it = table.find(req.path());
        if (it != table.end()) {
            it->second(req, res);
        } else {
            handle_static_file(conn, req, res);
        }
        break;
    }
    case HEAD:
    {
        handle_static_file(conn, req, res);
        break;
    }
    case POST:
    {
        auto& table = router[POST];
        auto it = table.find(req.path());
        if (it != table.end()) {
            it->second(req, res);
        } else {
            res.set_status_code(NotFound);
        }
        break;
    }
    default:
        res.set_status_code(NotImplemented);
        break;
    }

    bool keep_alive = util::equal_case(req.req_headers["Connection"], "keep-alive");
    res.add_header("Connection", keep_alive ? "keep-alive" : "close");

    if (!res.chunked) {
        conn->send(res.str());
    } else {
        res.chunked = false;
    }
    ctx.state = ParseLine;

    if (!keep_alive) {
        conn->close();
    }
}

static off_t get_file_size(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    return st.st_size;
}

static std::string get_last_modified(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    auto msec = st.st_mtimespec.tv_sec * 1000000 + st.st_mtimespec.tv_nsec / 1000;
    Clock::duration duration = std::chrono::microseconds(msec);
    Clock::time_point point(duration);
    return format_date(point);
}

void HttpServer::handle_static_file(const connection_ptr& conn, request& req, response& res)
{
    std::string path(base_dir + req.path());
    if (req.path() == "/") {
        path += "index.html";
    }
    if (util::is_regular_file(path)) {
        res.set_status_code(Ok);
        res.add_header("Last-Modified", get_last_modified(path));
        if (req.method() == GET) {
            send_file(conn, res, path);
        } else if (req.method() == HEAD) {
            res.add_header("Content-Length", std::to_string(get_file_size(path)));
        }
    } else {
        res.set_status_code(NotFound);
    }
}

static const int ChunkSize = 1024 * 1024 * 4; // 4M

static void send_chunk(const connection_ptr& conn, buffer& buf)
{
    char x[16];
    snprintf(x, sizeof(x), "%X\r\n", (unsigned)buf.readable());
    conn->send(x);
    buf.append("\r\n");
    conn->send(buf.peek(), buf.readable());
}

void HttpServer::send_file(const connection_ptr& conn, response& res, const std::string& path)
{
    buffer buf;
    res.chunked = false;
    int fd = open(path.c_str(), O_RDONLY);

    const char *mime_type = mimetypes.get_mime_type(path);
    mime_type = mime_type ? mime_type : "application/octet-stream";

    auto filesize = get_file_size(path);
    if (filesize >= ChunkSize) {
        res.add_header("Content-Type", mime_type);
        res.add_header("Transfer-Encoding", "chunked");
        conn->send(res.str());
        res.chunked = true;
    }

    while (buf.read_fd(fd) > 0) {
        if (buf.readable() >= ChunkSize) {
            send_chunk(conn, buf);
            buf.retrieve_all();
        }
    }
    if (buf.readable() > 0) {
        if (res.chunked) {
            send_chunk(conn, buf);
        } else {
            res.set_content(std::string_view(buf.peek(), buf.readable()), mime_type);
        }
    }
    if (res.chunked) {
        conn->send("0\r\n");
    }

    close(fd);
}

HttpServer& HttpServer::Get(std::string_view path, const ServerHandler handler)
{
    router[GET].emplace(path, std::move(handler));
    return *this;
}

HttpServer& HttpServer::Post(std::string_view path, const ServerHandler handler)
{
    router[POST].emplace(path, std::move(handler));
    return *this;
}

static const std::unordered_map<StatusCode, const char*> code_map = {
    { Continue,                     "Continue" },
    { SwitchingProtocols,           "Switching Protocols" },
    { Ok,                           "OK" },
    { Created,                      "Created" },
    { Accepted,                     "Accepted" },
    { NonAuthoritativeInformation,  "Non Authoritative Information" },
    { NoContent,                    "No Content" },
    { ResetContent,                 "Reset Content" },
    { PartialContent,               "Partial Content" },
    { MultipleChoices,              "Multiple Choices" },
    { MovedPermanently,             "Moved Permanently" },
    { Found,                        "Found" },
    { SeeOther,                     "See Other" },
    { NotModified,                  "Not Modified" },
    { UseProxy,                     "Use Proxy" },
    { TemporaryRedirect,            "Temporary Redirect" },
    { BadRequest,                   "Bad Request" },
    { Unauthorized,                 "Unauthorized" },
    { Forbidden,                    "Forbidden" },
    { NotFound,                     "Not Found" },
    { MethodNotAllowed,             "Method Not Allowed" },
    { NotAcceptable,                "Not Acceptable" },
    { ProxyAuthenticationRequired,  "Proxy Authentication Required" },
    { RequestTimeout,               "Request Timeout" },
    { Conflict,                     "Conflict" },
    { Gone,                         "Gone" },
    { LengthRequired,               "Length Required" },
    { PreconditionFailed,           "Precondition Failed" },
    { RequestEntityTooLarge,        "Request Entity Too Large" },
    { RequestUriTooLong,            "Request-URI Too Long" },
    { UnsupportedMediaType,         "Unsupported Media Type" },
    { RequestedRangeNotSatisfiable, "Requested Range Not Satisfiable" },
    { ExpectationFailed,            "Expectation Failed" },
    { InternalServerError,          "Internal Server Error" },
    { NotImplemented,               "Not Implemented" },
    { BadGateway,                   "Bad Gateway" },
    { ServiceUnavailable,           "Service Unavailable" },
    { GatewayTimeout,               "Gateway Timeout" },
    { HttpVersionNotSupported,      "Http Version Not Supported" },
};

const char *to_str(StatusCode code)
{
    auto it = code_map.find(code);
    return it != code_map.end() ? it->second : nullptr;
}

}
}
