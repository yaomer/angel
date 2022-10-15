#include <angel/httplib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <angel/mime.h>

namespace angel {
namespace httplib {

static const char SP    = ' ';
static const char *CRLF = "\r\n";
static const char *SEP  = ": "; // field <SEP> value

static const int uri_max_len = 1024 * 1024;

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

// Return Code for parse request:
// Ok: parse complete
// Continue: data not enough
// Other: there is an error occurs.

// Request-Line = Method <SP> Request-URI <SP> HTTP-Version <CRLF>
StatusCode request::parse_line(buffer& buf)
{
    int crlf = buf.find_crlf();
    if (crlf < 0) return Continue;

    if (crlf > uri_max_len) {
        return RequestUriTooLong;
    }

    auto res = util::split({buf.peek(), (size_t)crlf}, SP);
    if (res.size() != 3) return BadRequest;

    // Parse Method
    auto it = methods.find(res[0]);
    if (it == methods.end()) return BadRequest;
    req_method = it->second;

    // Parse Request-URI
    const char *p = res[1].data();
    const char *end = p + res[1].size();

    const char *sep = std::find(p, end, '?');
    req_path = uri::decode({p, (size_t)(sep - p)});

    if (sep != end) { // have parameters
        auto args = util::split({sep + 1, (size_t)(end - sep - 1)}, '&');
        for (auto& arg : args) {
            sep = std::find(arg.begin(), arg.end(), '=');
            if (sep == arg.end()) return BadRequest;
            std::string_view key(arg.begin(), (size_t)(sep - arg.begin()));
            std::string_view value(sep + 1, (size_t)(arg.end() - sep - 1));
            req_params.emplace(uri::decode(key), uri::decode(value));
        }
    }

    // Parse HTTP-Version
    if (res[2].size() != 8 || !util::starts_with(res[2], "HTTP/"))
        return BadRequest;
    if (util::ends_with(res[2], "1.0") || util::ends_with(res[2], "1.1")) {
        req_version = res[2];
    } else if (util::ends_with(res[2], "2.0")) {
        return HttpVersionNotSupported;
    } else {
        return BadRequest;
    }

    buf.retrieve(crlf + 2);
    return Ok;
}

// message-header = field-name ":" [ field-value ]
StatusCode request::parse_header(buffer& buf)
{
    while (buf.readable() > 0) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;

        if (buf.starts_with(CRLF)) {
            buf.retrieve(crlf + 2);
            if (headers().count("Host")) return Ok;
            return BadRequest;
        }

        int pos = buf.find(":");
        if (pos < 0) return BadRequest;

        std::string_view field(buf.peek(), pos);
        std::string_view value(util::trim({buf.peek() + pos + 1, (size_t)(crlf - pos - 1)}));
        // Ignore header with null value.
        if (!value.empty()) {
            req_headers.emplace(field, value);
        }

        buf.retrieve(crlf + 2);
    }
    return Continue;
}

StatusCode request::parse_body_length()
{
    // The transfer-length of that body is determined by
    // prefer use Transfer-Encoding header field.
    auto it = headers().find("Transfer-Encoding");
    if (it != headers().end()) {
        if (it->second == "chunked") {
            chunked = true;
            return Ok;
        }
    }

    it = headers().find("Content-Length");
    if (it == headers().end()) return Continue;

    auto r = util::svtoll(it->second);
    if (r.value_or(-1) < 0) return BadRequest;

    length = r.value();
    return length > 0 ? Ok : Continue;
}

StatusCode request::parse_body(buffer& buf)
{
    return chunked ? parse_body_by_chunked(buf) : parse_body_by_content_length(buf);
}

StatusCode request::parse_body_by_content_length(buffer& buf)
{
    if (buf.readable() >= length) {
        req_body.append(buf.peek(), length);
        buf.retrieve(length);
        return Ok;
    } else { // Not Enough
        req_body.append(buf.peek(), buf.readable());
        length -= buf.readable();
        buf.retrieve_all();
        return Continue;
    }
}

// Chunked Transfer Coding
//
// Chunked-Body   = *chunk
//                  last-chunk
//                  trailer
//                  CRLF
//
// chunk          = chunk-size [ chunk-extension ] CRLF
//                  chunk-data CRLF
// chunk-size     = 1*HEX
// last-chunk     = 1*("0") [ chunk-extension ] CRLF

static ssize_t from_hex_str(const std::string& str)
{
    char *end;
    auto chunk_size = std::strtoll(str.c_str(), &end, 16);
    if (end == str.data() + str.size()) {
        return chunk_size;
    } else {
        return -1;
    }
}

StatusCode request::parse_body_by_chunked(buffer& buf)
{
    while (buf.readable() > 0) {
        // Parse chunk-size <CRLF>
        if (chunk_size == -1) {
            int crlf = buf.find_crlf();
            if (crlf < 0) return Continue;
            chunk_size = from_hex_str({buf.peek(), (size_t)crlf});
            buf.retrieve(crlf + 2);
            if (chunk_size < 0) return BadRequest;
            if (chunk_size == 0) { // last-chunk
                chunk_size = -1;
                chunked = false;
                return Ok;
            }
        }
        // chunk-data <CRLF>
        if (buf.readable() > chunk_size) {
            if (buf.readable() < chunk_size + 2) return Continue;
            req_body.append(buf.peek(), chunk_size);
            buf.retrieve(chunk_size);
            if (!buf.starts_with(CRLF)) return BadRequest;
            buf.retrieve(2);
            chunk_size = -1;
        } else { // Not Enough
            req_body.append(buf.peek(), buf.readable());
            chunk_size -= buf.readable();
            buf.retrieve_all();
            return Continue;
        }
    }
    return Continue;
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

static void format_header(std::string& buf, std::string_view field, std::string_view value)
{
    buf.append(field).append(SEP).append(value).append(CRLF);
}

// Status-Line = HTTP-Version <SP> Status-Code <SP> Reason-Phrase <CRLF>
void response::append_status_line()
{
    buf.append("HTTP/1.1").append(1, SP)
       .append(std::to_string(status_code)).append(1, SP)
       .append(status_message).append(CRLF);
}

std::string& response::str()
{
    buf.clear();

    append_status_line();

    add_header("Server", "angel");
    add_header("Date", format_date());
    add_header("Content-Length", std::to_string(content.size()));

    for (auto& [field, value] : headers) {
        format_header(buf, field.val, value);
    }
    buf.append(CRLF);
    buf.append(content);

    headers.clear();
    content.clear();

    return buf;
}

void HttpServer::message_handler(const connection_ptr& conn, buffer& buf)
{
    StatusCode code;
    if (!conn->is_connected()) return;
    auto& ctx = std::any_cast<context&>(conn->get_context());
    auto& req = ctx.request;
    // printf("%s\n", buf.c_str());
    while (buf.readable() > 0) {
        switch (ctx.state) {
        case ParseLine:
            switch (code = req.parse_line(buf)) {
            case Ok:
                ctx.state = ParseHeader;
                break;
            case Continue:
                return;
            default:
                goto err;
            }
            break;
        case ParseHeader:
            switch (code = req.parse_header(buf)) {
            case Ok:
                switch (code = req.parse_body_length()) {
                case Ok:
                    ctx.state = ParseBody;
                    break;
                case Continue:
                    process_request(conn);
                    break;
                default:
                    goto err;
                }
                break;
            case Continue:
                return;
            default:
                goto err;
            }
            break;
        case ParseBody:
            switch (code = req.parse_body(buf)) {
            case Ok:
                process_request(conn);
                break;
            case Continue:
                return;
            default:
                goto err;
            }
            break;
        }
    }
    return;
err:
    ctx.response.set_status_code(code);
    ctx.response.add_header("Connection", "close");
    conn->send(ctx.response.str());
    conn->close();
}

void HttpServer::process_request(const connection_ptr& conn)
{
    auto& ctx = std::any_cast<context&>(conn->get_context());
    auto& req = ctx.request;
    auto& res = ctx.response;

    auto connection = req.value_or("Connection", "close");
    bool keepalive = util::equal_case(connection, "keep-alive");
    res.add_header("Connection", keepalive ? "keep-alive" : "close");

    switch (req.method()) {
    case GET:
        if (!handle_register_request(conn, req, res)) {
            handle_static_file_request(conn, req, res);
        }
        break;
    case HEAD:
        handle_static_file_request(conn, req, res);
        break;
    case POST:
        if (!handle_register_request(conn, req, res)) {
            res.set_status_code(NotFound);
            conn->send(res.str());
        }
        break;
    default:
        res.set_status_code(NotImplemented);
        conn->send(res.str());
        break;
    }

    ctx.state = ParseLine;
    req.clear();

    if (!keepalive) {
        conn->close();
    }
}

bool HttpServer::handle_register_request(const connection_ptr& conn, request& req, response& res)
{
    auto& table = router.at(req.method());
    auto it = table.find(req.path());
    if (it == table.end()) return false;
    it->second(req, res);
    conn->send(res.str());
    return true;
}

void HttpServer::handle_static_file_request(const connection_ptr& conn, request& req, response& res)
{
    if (req.path() == "/") {
        req.req_path = base_dir + req.path() + "index.html";
    } else {
        req.req_path = base_dir + req.path();
    }

    if (util::is_regular_file(req.path())) {
        res.add_header("Last-Modified", get_last_modified(req.path()));
        res.add_header("Accept-Ranges", "bytes");
        if (req.method() == GET) {
            if (req.headers().count("Range")) {
                handle_range_request(conn, req, res);
            } else {
                send_file(conn, res, req.path());
            }
        } else if (req.method() == HEAD) {
            size_t length = util::get_file_size(req.path());
            res.add_header("Content-Length", std::to_string(length));
        }
    } else {
        res.set_status_code(NotFound);
        conn->send(res.str());
    }
}

void HttpServer::send_file(const connection_ptr& conn, response& res, const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY);
    auto filesize = util::get_file_size(fd);

    res.set_status_code(Ok);
    res.add_header("Content-Type", get_mime_type(path));

    // Send small files directly.
    if (filesize < 256 * 1024) {
        buffer buf;
        while (buf.read_fd(fd) > 0) ;
        res.set_content({buf.peek(), buf.readable()});
        conn->send(res.str());
        close(fd);
        return;
    }

    res.add_header("Content-Length", std::to_string(filesize));
    conn->send(res.str());
    conn->send_file(fd, 0, filesize);
    conn->set_send_complete_handler([fd](const connection_ptr& conn){ close(fd); });
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

    std::string to_str();
    off_t length() { return last_byte_pos - first_byte_pos + 1; }
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
static const char *bytes_unit = "bytes";

std::string byte_range::to_str()
{
    std::string s(std::to_string(first_byte_pos));
    s.append("-").append(std::to_string(last_byte_pos));
    return s;
}

static std::string content_range(std::string_view resp, off_t filesize)
{
    std::string range_spec;
    range_spec.append(bytes_unit).append(1, SP)
              .append(resp).append("/").append(std::to_string(filesize));
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
    size_t len = range.length();
    char *start = reinterpret_cast<char*>(mmap(nullptr, len + diff, PROT_READ, MAP_SHARED, fd, offset));
    if (start == MAP_FAILED) return false;
    buf.append(start + diff, len).append(CRLF);
    munmap(start, len + diff);
    return true;
}

static const int BufferedSize = 1024 * 256;

// Range: bytes=0-100,-100\r\n
void HttpServer::handle_range_request(const connection_ptr& conn, request& req, response& res)
{
    byte_range_set range_set;

    range_set.filesize = util::get_file_size(req.path());
    range_set.mime_type = get_mime_type(req.path());

    // It's an empty file and hardly appears.
    if (range_set.filesize == 0) {
        res.set_status_code(Ok);
        res.add_header("Content-Type", range_set.mime_type);
        conn->send(res.str());
        return;
    }

    StatusCode code = range_set.parse_byte_ranges(req);
    switch (code) {
    case Ok:
        send_file(conn, res, req.path());
        return;
    case RequestedRangeNotSatisfiable:
        res.set_status_code(RequestedRangeNotSatisfiable);
        res.add_header("Content-Range", content_range("*", range_set.filesize));
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
    auto r = util::svtoll(s);
    if (r.has_value()) return r.value();
    return -1;
}

bool byte_range_set::parse_byte_range_spec(byte_range& range, std::string_view spec)
{
    auto byte_range_spec = util::split(spec, '-');
    if (byte_range_spec.size() != 2) return false;

    range.first_byte_pos = __byte_pos(util::trim(byte_range_spec[0]));
    if (range.first_byte_pos < 0 || range.first_byte_pos >= filesize) return false;

    if (!byte_range_spec[1].empty()) { // bytes=0-499
        range.last_byte_pos = __byte_pos(util::trim(byte_range_spec[1]));
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
    range.last_byte_pos  = filesize - 1;
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
        len += field_range.size() + sep + content_range(range.to_str(), filesize).size() + crlf;
        len += crlf;
        len += range.length() + crlf;
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

    if (ranges.size() == 1) {
        auto& range = ranges.back();
        res.add_header("Content-Type", mime_type);
        res.add_header("Content-Range", content_range(range.to_str(), filesize));
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
        format_header(buf, "Content-Type", mime_type);
        format_header(buf, "Content-Range", content_range(range.to_str(), filesize));
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

void HttpServer::start()
{
    set_base_dir(".");
    server.set_nodelay(true);
    server.set_keepalive(true);
    server.start();
}

}
}
