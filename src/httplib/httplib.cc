#include <angel/httplib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <chrono>
#include <iomanip>
#include <sstream>

#include <angel/mime.h>

namespace angel {
namespace httplib {

static const char SP = ' ';
static const char *CRLF = "\r\n";
static const char *SEP = ": "; // field <SEP> value

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

static std::string get_last_modified(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    auto msec = st.st_mtimespec.tv_sec * 1000000 + st.st_mtimespec.tv_nsec / 1000;
    Clock::duration duration = std::chrono::microseconds(msec);
    Clock::time_point point(duration);
    return format_date(point);
}

static mime::mimetypes mimetypes;

static const char *get_mime_type(const std::string& path)
{
    const char *mime_type = mimetypes.get_mime_type(path);
    return mime_type ? mime_type : "application/octet-stream";
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
    const char *next = std::find(p, linep, SP);
    if (next == linep) return BadRequest;

    std::string_view method(p, next - p);
    auto it = methods.find(method);
    if (it == methods.end()) return BadRequest;
    req_method = it->second;
    p = next + 1;

    // Parse Request-URI
    next = std::find(p, linep, SP);
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
    if (buf.starts_with(CRLF)) {
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
    size_t content_length = std::stoul(req_headers["Content-Length"]);
    req_body.assign(buf.peek(), content_length);
    buf.retrieve(content_length);
}

void request::clear()
{
    req_body.clear();
    req_params.clear();
    req_headers.clear();
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

    buf.append("HTTP/1.1").append(1, SP)
       .append(std::to_string(status_code)).append(1, SP)
       .append(status_message).append(CRLF);

    if (!content.empty()) {
        add_header("Content-Length", std::to_string(content.size()));
    }
    add_header("Date", format_date());

    for (auto& [field, value] : headers) {
        buf.append(field.val).append(SEP).append(value).append(CRLF);
    }
    buf.append(CRLF);
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
    if (base_dir.back() == '/') base_dir.pop_back();
}

void HttpServer::set_parallel(unsigned n)
{
    if (n == 0) return;
    server.start_io_threads(n);
}

void HttpServer::start()
{
    set_base_dir(".");
    server.set_nodelay(true);
    server.set_keepalive(true);
    server.start();
}

void HttpServer::message_handler(const connection_ptr& conn, buffer& buf)
{
    StatusCode code;
    if (!conn->is_connected()) return;
    auto& ctx = std::any_cast<context&>(conn->get_context());
    // printf("%s\n", buf.c_str());
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (ctx.state) {
        case ParseLine:
        {
            if (crlf > uri_max_len) {
                code = RequestUriTooLong;
                goto err;
            }
            code = ctx.request.parse_line(buf, crlf);
            if (code != Ok) {
                goto err;
            }
            ctx.state = ParseHeader;
            break;
        }
        case ParseHeader:
        {
            code = ctx.request.parse_header(buf, crlf);
            switch (code) {
            case Ok:
                if (ctx.request.method() == POST) {
                    if (!ctx.request.headers().count("Content-Length")) {
                        code = LengthRequired;
                        goto err;
                    }
                    ctx.request.parse_body(buf);
                }
                process_request(conn);
                break;
            case Continue:
                break;
            default:
                goto err;
            }
            break;
        }
        }
    }
    return;
err:
    ctx.response.set_status_code(code);
    conn->send(ctx.response.str());
    conn->close();
}

void HttpServer::process_request(const connection_ptr& conn)
{
    auto& ctx = std::any_cast<context&>(conn->get_context());
    auto& req = ctx.request;
    auto& res = ctx.response;

    res.add_header("Server", "angel");

    bool keep_alive = util::equal_case(req.headers().at("Connection"), "keep-alive");
    res.add_header("Connection", keep_alive ? "keep-alive" : "close");

    switch (req.method()) {
    case GET:
    {
        auto& table = router[GET];
        auto it = table.find(req.path());
        if (it != table.end()) {
            it->second(req, res);
            conn->send(res.str());
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
            conn->send(res.str());
        } else {
            res.set_status_code(NotFound);
            conn->send(res.str());
        }
        break;
    }
    default:
        res.set_status_code(NotImplemented);
        conn->send(res.str());
        break;
    }

    ctx.state = ParseLine;
    req.clear();

    if (!keep_alive) {
        conn->close();
    }
}

static const int ChunkSize = 1024 * 512;

static void send_chunk(const connection_ptr& conn, buffer& buf)
{
    char x[16];
    snprintf(x, sizeof(x), "%X%s", (unsigned)buf.readable(), CRLF);
    conn->send(x);
    buf.append(CRLF);
    conn->send(buf.peek(), buf.readable());
}

void HttpServer::send_file(const connection_ptr& conn, response& res, const std::string& path)
{
    buffer buf;
    bool chunked = false;
    int fd = open(path.c_str(), O_RDONLY);

    res.set_status_code(Ok);
    res.add_header("Content-Type", get_mime_type(path));

    auto filesize = util::get_file_size(path);

    if (filesize >= ChunkSize) {
        res.add_header("Transfer-Encoding", "chunked");
        conn->send(res.str());
        chunked = true;
    }

    while (buf.read_fd(fd) > 0) {
        if (buf.readable() >= ChunkSize) {
            send_chunk(conn, buf);
            buf.retrieve_all();
        }
    }
    if (buf.readable() > 0) {
        if (chunked) {
            send_chunk(conn, buf);
        } else {
            res.set_content({buf.peek(), buf.readable()});
            conn->send(res.str());
        }
    }
    if (chunked) {
        std::string final_chunk;
        final_chunk.append("0").append(CRLF);
        conn->send(final_chunk);
    }

    close(fd);
}

// Byte Ranges
// ranges-specifier = byte-ranges-specifier
// byte-ranges-specifier = bytes-unit "=" byte-range-set
// byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
// byte-range-spec = first-byte-pos "-" [last-byte-pos] ; Byte offsets start at zero
// suffix-byte-range-spec = "-" suffix-length
//
// Assuming an entity-body of length 1000:
// bytes=0-499  (first 500 bytes)
// bytes=0-     (all bytes)
// bytes=-500   (final 500 bytes) or bytes=500-999
// bytes=0-0,-1 (first and last byte only)

struct byte_range {
    off_t first_byte_pos;
    off_t last_byte_pos;
};

struct byte_range_set {
    off_t filesize;
    std::string_view mime_type;
    std::vector<byte_range> ranges;
    std::string boundary;

    StatusCode parse_byte_ranges(request& req);
    bool parse_byte_range_spec(byte_range& range, std::string_view spec);
    bool parse_suffix_byte_range_spec(byte_range& range, std::string_view spec);

    void build_multipart_header(response& res);
    void send_range_response(const connection_ptr& conn, request& req, response& res);
};

// Content-Range = "Content-Range" ":" content-range-spec
//
// content-range-spec      = byte-content-range-spec
// byte-content-range-spec = bytes-unit SP
//                           byte-range-resp-spec "/"
//                           ( instance-length | "*" )
// byte-range-resp-spec    = (first-byte-pos "-" last-byte-pos) | "*"
//
// Accept-Ranges     = "Accept-Ranges" ":" acceptable-ranges
// acceptable-ranges = 1#range-unit | "none"
//
static const char *bytes_unit = "bytes";

static std::string content_range(byte_range& range, off_t filesize)
{
    std::string range_spec;
    range_spec.append(bytes_unit).append(1, SP)
              .append(std::to_string(range.first_byte_pos)).append("-")
              .append(std::to_string(range.last_byte_pos))
              .append("/")
              .append(std::to_string(filesize));
    return range_spec;
}

static off_t __map_offset(off_t offset)
{
    static const int pagesize = getpagesize();

    return (offset / pagesize) * pagesize;
}

static bool map_file_range(std::string& buf, int fd, byte_range& range)
{
    off_t offset = __map_offset(range.first_byte_pos);
    off_t diff = range.first_byte_pos - offset;
    size_t len = range.last_byte_pos - range.first_byte_pos + 1;
    char *start = reinterpret_cast<char*>(mmap(nullptr, len + diff, PROT_READ, MAP_SHARED, fd, offset));
    if (start == MAP_FAILED) return false;
    buf.append(start + diff, len).append(CRLF);
    munmap(start, len + diff);
    return true;
}

static const int BufferedSize = ChunkSize;

// Range: bytes=0-100,-100\r\n
void HttpServer::process_range_request(const connection_ptr& conn, request& req, response& res)
{
    byte_range_set range_set;

    range_set.filesize = util::get_file_size(req.path());
    range_set.mime_type = get_mime_type(req.path());

    StatusCode code = range_set.parse_byte_ranges(req);
    switch (code) {
    case Ok:
        send_file(conn, res, req.path());
        return;
    case RequestedRangeNotSatisfiable:
        res.set_status_code(RequestedRangeNotSatisfiable);
        conn->send(res.str());
        conn->close();
        return;
    case PartialContent:
        range_set.send_range_response(conn, req, res);
        break;
    default:
        abort();
    }
}

// Return:
// PartialContent: valid range
// RequestedRangeNotSatisfiable: invalid range
// Ok: ignore the range request and return the entire file.
StatusCode byte_range_set::parse_byte_ranges(request& req)
{
    assert(filesize > 0);
    std::string_view range(req.headers().at("Range"));
    if (!util::starts_with(range, "bytes=")) return Ok;
    range.remove_prefix(6);
    if (range.empty()) return Ok;

    auto res = util::split(range, ',');
    for (auto& spec : res) {
        byte_range range;
        spec = util::trim(spec);
        // A valid byte-range-spec has 2-byte at least. (0-, -1)
        if (spec.size() < 2) return RequestedRangeNotSatisfiable;
        if (spec[0] == '-') {
            spec.remove_prefix(1);
            if (!isdigit(spec[0]) || !parse_suffix_byte_range_spec(range, spec))
                return RequestedRangeNotSatisfiable;
        } else {
            if (!parse_byte_range_spec(range, spec))
                return RequestedRangeNotSatisfiable;
        }
        ranges.emplace_back(std::move(range));
    }
    return PartialContent;
}

// Return -1 if an error occurs.
static off_t __byte_pos(std::string_view s)
{
    off_t pos;
    // byte-pos is not allowed to be negative
    if (s[0] == '-') return -1;
    auto res = std::from_chars(s.data(), s.data() + s.size(), pos);
    // Must be completely converted, such as "12a" is wrong.
    if (res.ec == std::errc() && res.ptr == s.data() + s.size())
        return pos;
    return -1;
}

bool byte_range_set::parse_byte_range_spec(byte_range& range, std::string_view spec)
{
    auto byte_range_spec = util::split(spec, '-');
    if (byte_range_spec.size() != 2) return false;

    range.first_byte_pos = __byte_pos(util::trim(byte_range_spec[0]));
    if (range.first_byte_pos < 0 || range.first_byte_pos >= filesize) return false;

    if (!byte_range_spec[1].empty()) { // bytes=0-499
        range.last_byte_pos  = __byte_pos(util::trim(byte_range_spec[1]));
        if (range.last_byte_pos < 0 || range.last_byte_pos < range.first_byte_pos) return false;
        if (range.last_byte_pos >= filesize) range.last_byte_pos = filesize - 1;
    } else { // bytes=0-
        range.last_byte_pos = filesize - 1;
    }
    return true;
}

bool byte_range_set::parse_suffix_byte_range_spec(byte_range& range, std::string_view spec)
{
    off_t suffix_length = __byte_pos(spec);
    if (suffix_length <= 0) return false;
    suffix_length = std::min(suffix_length, filesize);
    range.first_byte_pos = filesize - suffix_length;
    range.last_byte_pos = filesize - 1;
    return true;
}

void byte_range_set::build_multipart_header(response& res)
{
    static const int crlf = strlen(CRLF);
    static const int sep  = strlen(SEP);
    static const std::string_view field_type("Content-Type");
    static const std::string_view field_range("Content-Range");

    boundary = mime::generate_boundary();
    std::string content_type("multipart/byteranges");
    content_type.append("; ").append("boundary=\"").append(boundary).append("\"");
    res.add_header("Content-Type", content_type);

    size_t len = 0;
    for (auto& range : ranges) {
        // "--" boundary <CRLF>
        len += 2 + boundary.size() + crlf;
        len += field_type.size() + sep + mime_type.size() + crlf;
        len += field_range.size() + sep + content_range(range, filesize).size() + crlf;
        len += crlf;
        len += range.last_byte_pos - range.first_byte_pos + 1 + crlf;
    }
    // Last boundary = "--" boundary "--" <CRLF>
    len += 2 + boundary.size() + 2 + crlf;

    res.add_header("Content-Length", std::to_string(len));
}

void byte_range_set::send_range_response(const connection_ptr& conn, request& req, response& res)
{
    std::string buf;
    int fd = open(req.path().c_str(), O_RDONLY);

    res.set_status_code(PartialContent);
    res.add_header("Accept-Ranges", bytes_unit);

    if (ranges.size() == 1) {
        auto& range = ranges.back();
        res.add_header("Content-Type", mime_type);
        res.add_header("Content-Range", content_range(range, filesize));
        if (!map_file_range(res.content, fd, range)) {
            res.headers.clear();
            res.set_status_code(InternalServerError);
        }
        conn->send(res.str());
        goto end;
    }

    build_multipart_header(res);
    conn->send(res.str());

    // Build and send each entity part in sequence.
    for (auto& range : ranges) {
        buf.append("--").append(boundary).append(CRLF);
        buf.append("Content-Type").append(SEP).append(mime_type).append(CRLF);
        buf.append("Content-Range").append(SEP).append(content_range(range, filesize)).append(CRLF);
        buf.append(CRLF);
        if (!map_file_range(buf, fd, range)) {
            res.set_status_code(InternalServerError);
            conn->send(res.str());
            goto end;
        }
        if (buf.size() >= BufferedSize) {
            conn->send(buf);
            buf.clear();
        }
    }
    buf.append("--").append(boundary).append("--").append(CRLF);
    conn->send(buf);
end:
    close(fd);
}

void HttpServer::handle_static_file(const connection_ptr& conn, request& req, response& res)
{
    if (req.path() == "/") {
        req.req_path = base_dir + req.path() + "index.html";
    } else {
        req.req_path = base_dir + req.path();
    }

    if (util::is_regular_file(req.path())) {
        res.add_header("Last-Modified", get_last_modified(req.path()));
        if (req.method() == GET) {
            if (req.headers().count("Range")) {
                process_range_request(conn, req, res);
            } else {
                send_file(conn, res, req.path());
            }
        } else if (req.method() == HEAD) {
            res.add_header("Content-Length", std::to_string(util::get_file_size(req.path())));
        }
    } else {
        res.set_status_code(NotFound);
        conn->send(res.str());
    }
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
