#include <angel/httplib.h>

#include <unistd.h>
#include <fcntl.h>

#include <angel/mime.h>
#include <angel/config.h>

#if defined (ANGEL_USE_OPENSSL)
#include <angel/ssl_client.h>
#endif

#include "util.h"

namespace angel {
namespace httplib {

static const char SP    = ' ';
static const char *CRLF = "\r\n";
static const char *SEP  = ": "; // field <SEP> value

static mime::mimetypes mimetypes;

static const char *get_mime_type(const std::string& path)
{
    const char *mime_type = mimetypes.get_mime_type(path);
    return mime_type ? mime_type : "application/octet-stream";
}

// HTTP-message = Request | Response
//
// Request and Response messages use the generic message format of
// RFC 822 for transferring entities (the payload of the message).
//
// generic-message = start-line
//                 *(message-header CRLF)
//                 CRLF
//                 [ message-body ]
// start-line    = Request-Line | Status-Line

// Return Code for parsing:
// Ok: parse complete
// Continue: data not enough
// Other: there is an error occurs.

// message-header = field-name ":" [ field-value ]
StatusCode message::parse_header(buffer& buf)
{
    while (buf.readable() > 0) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;

        if (crlf == 0) {
            buf.retrieve(2);
            return Ok;
        }

        int pos = buf.find(":");
        if (pos < 0) return BadRequest;

        std::string_view field(buf.peek(), pos);
        std::string_view value(util::trim({buf.peek() + pos + 1, (size_t)(crlf - pos - 1)}));
        // Ignore header with null value.
        if (!value.empty()) {
            headers.emplace(field, value);
        }

        buf.retrieve(crlf + 2);
    }
    return Continue;
}

StatusCode message::parse_body_length()
{
    // The transfer-length of that body is determined by
    // prefer use Transfer-Encoding header field.
    auto it = headers.find("Transfer-Encoding");
    if (it != headers.end()) {
        if (it->second == "chunked") {
            chunked = true;
            return Ok;
        }
    }

    it = headers.find("Content-Length");
    if (it == headers.end()) return Continue;

    auto r = util::svtoll(it->second);
    if (r.value_or(-1) < 0) return BadRequest;

    length = r.value();
    return length > 0 ? Ok : Continue;
}

StatusCode message::parse_body(buffer& buf)
{
    return chunked ? parse_body_by_chunked(buf) : parse_body_by_content_length(buf);
}

StatusCode message::parse_body_by_content_length(buffer& buf)
{
    if (buf.readable() >= length) {
        body.append(buf.peek(), length);
        buf.retrieve(length);
        return Ok;
    } else { // Not Enough
        body.append(buf.peek(), buf.readable());
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
//
// chunk-data     = chunk-size(OCTET)
// trailer        = *(entity-header CRLF)

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

StatusCode message::parse_body_by_chunked(buffer& buf)
{
    while (buf.readable() > 0) {
        // Parse chunk-size <CRLF>
        if (chunk_size == -1) {
            int crlf = buf.find_crlf();
            if (crlf < 0) break;
            if (buf.readable() < crlf + 4) break;
            chunk_size = from_hex_str({buf.peek(), (size_t)crlf});
            buf.retrieve(crlf + 2);
            if (chunk_size < 0) return BadRequest;
            // last-chunk
            if (chunk_size == 0) continue;
        } else if (chunk_size == 0) {
            // Parse trailer <CRLF>
            auto code = parse_header(buf);
            if (code != Ok) return code;
            chunk_size = -1;
            chunked = false;
            return Ok;
        }
        // chunk-data <CRLF>
        if (buf.readable() > chunk_size) {
            if (buf.readable() < chunk_size + 2) break;
            body.append(buf.peek(), chunk_size);
            buf.retrieve(chunk_size);
            if (!buf.starts_with(CRLF)) return BadRequest;
            buf.retrieve(2);
            chunk_size = -1;
        } else { // Not Enough
            body.append(buf.peek(), buf.readable());
            chunk_size -= buf.readable();
            buf.retrieve_all();
            break;
        }
    }
    return Continue;
}

void message::clear()
{
    headers.clear();
    body.clear();
}

//=================================================
//===================== Uri =======================
//=================================================

std::string uri::encode(std::string_view uri)
{
    std::string res;
    size_t n = uri.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char c = uri[i];
        if (!isalnum(c) && !strchr("-_.~", c)) {
            mime::codec::to_hex_pair(res, '%', c);
        } else {
            res.push_back(c);
        }
    }
    return res;
}

bool uri::decode(std::string_view uri, std::string& res)
{
    size_t n = uri.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char c = uri[i];
        if (c == '%') {
            if (i + 2 >= n) return false;
            char c1 = uri[++i];
            char c2 = uri[++i];
            if (!ishexnumber(c1) || !ishexnumber(c2)) return false;
            c = mime::codec::from_hex_pair(c1, c2);
        }
        res.push_back(c);
    }
    return true;
}

// ?k1=v1&k2=v2&k3=v3
bool uri::parse_params(const char *first, const char *last, Params& params)
{
    auto args = util::split({first, (size_t)(last - first)}, '&');
    for (auto& arg : args) {
        auto sep = std::find(arg.begin(), arg.end(), '=');
        if (sep == arg.end()) return false;
        std::string_view key(arg.begin(), (size_t)(sep - arg.begin()));
        std::string_view value(sep + 1, (size_t)(arg.end() - sep - 1));
        std::string decoded_key, decoded_value;
        if (!decode(key, decoded_key) || !decode(value, decoded_value)) return false;
        params.emplace(std::move(decoded_key), std::move(decoded_value));
    }
    return true;
}

bool uri::parse(std::string_view uri)
{
    clear();

    const char *p = uri.data();
    const char *end = uri.data() + uri.size();

    auto sep = util::search(uri, "://");
    if (!sep) return false;
    scheme.assign(p, sep);
    p = sep + 3;

    if (scheme == "http") port = 80;
    else if (scheme == "https") port = 443;
    else return false;

    sep = std::find(p, end, ':');
    if (sep != end) { // http://host:port/
        host.assign(p, sep);
        p = sep + 1;
        sep = std::find(p, end, '/');
        port = util::svtoi({p, (size_t)(sep - p)}).value_or(-1);
        if (port <= 0) return false;
    } else { // http://host/
        sep = std::find(p, end, '/');
        host.assign(p, sep);
    }
    // http://host => http://host/
    if (sep == end) {
        path = "/";
        return true;
    }
    p = sep;
    sep = std::find(p, end, '?');
    if (!decode({p, (size_t)(sep - p)}, path)) return false;

    auto frag = std::find(p, end, '#');

    if (sep != end) { // have parameters
        if (!parse_params(sep + 1, frag, params)) return false;
    }

    if (frag != end) { // have #fragment
        fragment.assign(frag + 1, end);
    }

    return true;
}

void uri::clear()
{
    scheme.clear();
    host.clear();
    port = 80;
    path.clear();
    params.clear();
    fragment.clear();
}

//====================================================
//=================== http_server ====================
//====================================================

static const char *index_page()
{
    static const char *html =
        "<html>\n"
        "<head><title>angel-server</title></head>\n"
        "<body>\n"
        "<center><h1>Welcome to angel-server!</h1></center>\n"
        "</body>\n"
        "</html>\n";
    return html;
}

static const char *err_page(StatusCode code)
{
    static thread_local char buf[1024];
    static const char *html =
        "<html>\n"
        "<head><title>%d %s</title></head>\n"
        "<body>\n"
        "<center><h1>%d %s</h1></center>\n"
        "<hr><center>ange-server</center>\n"
        "</body>\n"
        "</html>";
    snprintf(buf, sizeof(buf), html, code, to_str(code), code, to_str(code));
    return buf;
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

static const int UriMaxLength = 1024 * 1024;

// Request-Line = Method <SP> Request-URI <SP> HTTP-Version <CRLF>
StatusCode request::parse_line(buffer& buf)
{
    int crlf = buf.find_crlf();
    if (crlf < 0) return Continue;

    if (crlf > UriMaxLength) {
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
    if (!uri::decode({p, (size_t)(sep - p)}, abs_path)) return BadRequest;

    if (sep != end) { // have parameters
        end = std::find(p, end, '#'); // ignore #fragment
        if (!uri::parse_params(sep + 1, end, query_params)) return BadRequest;
    }

    // Parse HTTP-Version
    if (res[2] == "HTTP/1.1") {
        http_version = HTTP_VERSION_1_1;
    } else if (res[2] == "HTTP/1.0") {
        http_version = HTTP_VERSION_1_0;
    } else if (res[2] == "HTTP/2.0") {
        return HttpVersionNotSupported;
    } else {
        return BadRequest;
    }

    buf.retrieve(crlf + 2);
    return Ok;
}

void request::clear()
{
    state = ParseLine;
    abs_path.clear();
    query_params.clear();
    message::clear();
}

void response::set_status_code(StatusCode code)
{
    status_code = code;
}

void response::add_header(std::string_view field, std::string_view value)
{
    headers.emplace(field, value);
}

void response::set_content(std::string_view body, std::string_view type)
{
    add_header("Content-Type", type);
    send(body);
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
       .append(to_str(status_code)).append(CRLF);
}

// Build Response-Header
std::string& response::header()
{
    buf.clear();

    append_status_line();

    add_header("Server", "angel");
    add_header("Date", format_date());

    for (auto& [field, value] : headers) {
        format_header(buf, field.key, value);
    }
    buf.append(CRLF);

    headers.clear();

    return buf;
}

static const int BufferedSize = 4096;

void response::send(std::string_view body)
{
    if (body.size() > 0)
        add_header("Content-Length", std::to_string(body.size()));
    if (body.size() >= BufferedSize) {
        conn->send(header());
        conn->send(body);
    } else {
        conn->send(header().append(body));
    }
}

void response::send_chunk(std::string_view chunk)
{
    if (!chunked) {
        chunked = true;
        add_header("Transfer-Encoding", "chunked");
        if (chunk.size() >= BufferedSize) {
            conn->send(header());
        } else {
            chunked_buf.append(header());
        }
    }
    char x[16];
    snprintf(x, sizeof(x), "%zx\r\n", chunk.size());
    if (chunk.size() >= BufferedSize) {
        conn->send(chunked_buf.append(x));
        conn->send(chunk);
        chunked_buf.clear();
        chunked_buf.append(CRLF);
    } else {
        chunked_buf.append(x).append(chunk).append(CRLF);
        if (chunked_buf.size() >= BufferedSize) {
            conn->send(chunked_buf);
            chunked_buf.clear();
        }
    }
}

void response::send_done()
{
    chunked = false;
    chunked_buf.append("0\r\n\r\n");
    conn->send(chunked_buf);
    chunked_buf.clear();
}

void response::send_err()
{
    add_header("Content-Type", "text/html");
    send(err_page(status_code));
}

void http_server::message_handler(const connection_ptr& conn, buffer& buf)
{
    StatusCode code;
    if (!conn->is_connected()) return;
    auto& ctx = std::any_cast<context&>(conn->get_context());
    auto& req = ctx.request;
    auto& res = ctx.response;
    // printf("%s\n", buf.c_str());
    while (buf.readable() > 0) {
        switch (req.state) {
        case ParseLine:
            switch (code = req.parse_line(buf)) {
            case Ok:
                req.state = ParseHeader;
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
                if (!req.headers().count("Host")) {
                    code = BadRequest;
                    goto err;
                }
                if (expect(req, res) == Failed) {
                    req.clear();
                    return;
                }

                switch (code = req.parse_body_length()) {
                case Ok:
                    req.state = ParseBody;
                    break;
                case Continue:
                    process_request(conn, req, res);
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
                process_request(conn, req, res);
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
    ctx.response.send_err();
    conn->close();
}

void http_server::process_request(const connection_ptr& conn, request& req, response& res)
{
    bool is_keepalive = keepalive(req);
    res.add_header("Connection", is_keepalive ? "keep-alive" : "close");

    switch (req.method()) {
    case GET:
        if (handle_user_router(req, res)) break;
        handle_static_file_request(req, res);
        break;
    case HEAD:
        handle_static_file_request(req, res);
        break;
    case POST:
        handle_user_router(req, res);
        break;
    case PUT:
        handle_static_file_request(req, res);
        break;
    case DELETE:
        handle_static_file_request(req, res);
        break;
    default:
        res.set_status_code(NotImplemented);
        res.send_err();
        break;
    }

    req.clear();

    if (!is_keepalive) {
        conn->close();
    }
}

bool http_server::keepalive(request& req)
{
    auto it = req.headers().find("Connection");
    if (it == req.headers().end()) {
        // HTTP/1.1 Keep-Alive by default
        return req.version() == HTTP_VERSION_1_1;
    }
    return util::equal_case(it->second, "keep-alive");
}

bool http_server::handle_user_router(request& req, response& res)
{
    auto& table = router.at(req.method());
    auto it = table.find(req.path());
    if (it == table.end()) return false;
    it->second(req, res);
    return true;
}

void http_server::handle_file_router(request& req, response& res)
{
    auto it = file_table.find(req.path());
    if (it != file_table.end()) {
        it->second(req, res.headers);
    }
}

void http_server::handle_static_file_request(request& req, response& res)
{
    bool root_index = false;
    if (req.path() == "/") {
        req.abs_path += "index.html";
        root_index = true;
    }
    req.abs_path = base_dir + req.path();
    req.has_file = util::is_regular_file(req.path());

    if (req.method() == PUT) {
        update_file(req, res);
        return;
    } else if (req.method() == DELETE) {
        delete_file(req, res);
        return;
    }

    if (!req.has_file) {
        if (root_index) {
            res.set_status_code(Ok);
            res.send(index_page());
        } else {
            res.set_status_code(NotFound);
            res.send_err();
        }
        return;
    }

    auto last_modified_time = get_last_modified(req.path());

    req.filesize = util::get_file_size(req.path());
    req.last_modified = format_last_modified(last_modified_time);
    if (generate_file_etag_by_sha1) {
        req.etag = generate_file_etag(req.path(), req.filesize);
    } else {
        req.etag = generate_file_etag(last_modified_time, req.filesize);
    }

    res.add_header("Last-Modified", req.last_modified);
    res.add_header("Accept-Ranges", "bytes");
    res.add_header("ETag", req.etag);

    if (handle_conditional(req, res) == Failed) return;

    if (req.method() == GET) {
        handle_file_router(req, res);
        if (req.headers().count("Range")) {
            handle_range_request(req, res);
        } else {
            send_file(req, res);
        }
    } else if (req.method() == HEAD) {
        res.set_status_code(Ok);
        res.add_header("Content-Length", std::to_string(req.filesize));
        res.send();
    }
}

// If-Match
// If-Modified-Since
// If-None-Match
// If-Range
// If-Unmodified-Since
//
// This several request-header fields are used with a method to
// make it conditional.
//
// The purpose of these features is to allow efficient updates of
// cached information with a minimum amount of transaction overhead.

// ETag/If-None-Match has a higher priority than Last-Modified/If-Modified-Since,
// And (If-None-Match, If-Modified-Since) and (If-Match, If-Unmodified-Since)
// are mutually exclusive.
//
// So we check the conditions in the following order:
// If-None-Match
// If-Modified-Since
// If-Match
// If-Unmodified-Since
//
ConditionCode http_server::handle_conditional(request& req, response& res)
{
    if_range(req, res);

    auto code = if_none_match(req, res);
    if (code != NoHeader) return code;

    code = if_modified_since(req, res);
    if (code != NoHeader) return code;

    code = if_match(req, res);
    if (code != NoHeader) return code;

    return if_unmodified_since(req, res);
}

// If-Match = "If-Match" ":" ( "*" | 1#entity-tag )
//
// It is used on updating requests, to prevent inadvertent
// modification of the wrong version of a resource.
ConditionCode http_server::if_match(request& req, response& res)
{
    auto it = req.headers().find("If-Match");
    if (it == req.headers().end()) return NoHeader;

    if (it->second == "*") return Successful;

    auto etags = util::split(it->second, ',');
    for (auto& etag : etags) {
        if (strong_etag_equal(etag, req.etag)) {
            return Successful;
        }
    }
    // None of the entity tags match.
    res.set_status_code(PreconditionFailed);
    res.send_err();
    return Failed;
}

// If-Modified-Since = "If-Modified-Since" ":" HTTP-date
//
// If the requested variant has not been modified since the time
// specified in this field, an entity will not be returned from
// the server; instead, a 304 (not modified) response will be
// returned without any message-body.
ConditionCode http_server::if_modified_since(request& req, response& res)
{
    auto it = req.headers().find("If-Modified-Since");
    if (it == req.headers().end()) return NoHeader;

    if (it->second != req.last_modified) {
        return Successful;
    }
    res.set_status_code(NotModified);
    res.send();
    return Failed;
}

// If-None-Match = "If-None-Match" ":" ( "*" | 1#entity-tag )
//
// It is used to prevent a method (e.g. PUT) from inadvertently
// modifying an existing resource when the client believes that
// the resource does not exist.
ConditionCode http_server::if_none_match(request& req, response& res)
{
    auto it = req.headers().find("If-None-Match");
    if (it == req.headers().end()) return NoHeader;

    auto etags = util::split(it->second, ',');

    auto *etag_equal = (req.method() == GET || req.method() == HEAD) ? weak_etag_equal : strong_etag_equal;

    if (it->second == "*") goto end;

    for (auto& etag : etags) {
        if (etag_equal(etag, req.etag)) {
            goto end;
        }
    }
    // If none of the entity tags match, then the server MAY perform the
    // requested method as if the If-None-Match header field did not exist,
    // but MUST also ignore any If-Modified-Since header field(s) in the
    // request.
    //
    // That is, if no entity tags match, then the server MUST NOT return
    // a 304 (Not Modified) response.
    //
    return Successful;
end:
    if (req.method() == GET || req.method() == HEAD) {
        res.set_status_code(NotModified);
        res.send();
    } else {
        res.set_status_code(PreconditionFailed);
        res.send_err();
    }
    return Failed;
}

// If-Range = "If-Range" ":" ( entity-tag | HTTP-date )
//
// If a client has a partial copy of an entity in its cache, and wishes
// to have an up-to-date copy of the entire entity in its cache, it
// could use the Range request-header with a conditional GET (using
// either or both of If-Unmodified-Since and If-Match.)
//
// However, if the condition fails because the entity has been modified,
// the client would then have to make a second request to obtain the
// entire current entity-body.
//
// The If-Range header allows a client to "short-circuit" the second
// request. Informally, its meaning is `if the entity is unchanged,
// send me the part(s) that I am missing; otherwise, send me the
// entire new entity`.
ConditionCode http_server::if_range(request& req, response& res)
{
    auto it = req.headers().find("If-Range");
    if (it == req.headers().end()) return NoHeader;

    // The If-Range header SHOULD only be used together with a Range header,
    // and MUST be ignored if the request does not include a Range header.
    if (!req.headers().count("Range")) return Successful;

    if (is_etag(it->second)) {
        if (strong_etag_equal(it->second, req.etag)) {
            return Successful;
        }
    } else {
        if (it->second == req.last_modified) {
            return Successful;
        }
    }
    // Perform it as if the Range header were not present.
    req.message::headers.erase("Range");
    return Successful;
}

// If-Unmodified-Since = "If-Unmodified-Since" ":" HTTP-date
//
// If the requested variant has been modified since the specified time,
// the server MUST NOT perform the requested operation, and MUST return
// a 412 (Precondition Failed).
ConditionCode http_server::if_unmodified_since(request& req, response& res)
{
    auto it = req.headers().find("If-Unmodified-Since");
    if (it == req.headers().end()) return NoHeader;

    if (it->second == req.last_modified) {
        return Successful;
    }
    res.set_status_code(PreconditionFailed);
    res.send_err();
    return Failed;
}

// Expect       =  "Expect" ":" 1#expectation
// expectation  =  "100-continue" | expectation-extension
//
// The Expect request-header field is used to indicate that particular
// server behaviors are required by the client.
ConditionCode http_server::expect(request& req, response& res)
{
    auto it = req.headers().find("Expect");
    if (it == req.headers().end()) return NoHeader;

    if (util::equal_case(it->second, "100-continue")) {
        return Successful;
    }
    res.set_status_code(ExpectationFailed);
    res.send_err();
    return Failed;
}

void http_server::send_file(request& req, response& res)
{
    int fd = open(req.path().c_str(), O_RDONLY);

    res.set_status_code(Ok);
    res.add_header("Content-Type", get_mime_type(req.path()));

    // Send small files directly.
    if (req.filesize < 256 * 1024) {
        buffer buf;
        while (buf.read_fd(fd) > 0) ;
        res.send({buf.peek(), buf.readable()});
        close(fd);
        return;
    }

    res.add_header("Content-Length", std::to_string(req.filesize));
    res.send();
    res.conn->send_file(fd, 0, req.filesize);
    res.conn->set_send_complete_handler([fd](const connection_ptr& conn){ close(fd); });
}

// Update or create a file
void http_server::update_file(request& req, response& res)
{
    int fd = open(req.path().c_str(), O_RDWR | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
        res.set_status_code(InternalServerError);
        res.send_err();
        return;
    }
    bool rc = util::write_file(fd, req.body().data(), req.body().size());
    if (!rc) {
        res.set_status_code(InternalServerError);
        res.send_err();
        close(fd);
        return;
    }
    res.set_status_code(req.has_file ? NoContent : Created);
    std::string location("http://");
    location.append(req.headers().at("Host"));
    location.append(req.path());
    res.add_header("Location", location);
    res.send();
    close(fd);
}

void http_server::delete_file(request& req, response& res)
{
    if (req.has_file) {
        ::unlink(req.path().c_str());
    }
    res.set_status_code(NoContent);
    res.send();
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
    void send_range_response(request& req, response& res);
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

// Range: bytes=0-100,-100\r\n
void http_server::handle_range_request(request& req, response& res)
{
    byte_range_set range_set;

    range_set.filesize = req.filesize;
    range_set.mime_type = get_mime_type(req.path());

    // It's an empty file and hardly appears.
    if (range_set.filesize == 0) {
        res.set_status_code(Ok);
        res.add_header("Content-Type", range_set.mime_type);
        res.add_header("Content-Length", "0");
        res.send();
        return;
    }

    StatusCode code = range_set.parse_byte_ranges(req);
    switch (code) {
    case Ok:
        send_file(req, res);
        return;
    case RequestedRangeNotSatisfiable:
        res.set_status_code(RequestedRangeNotSatisfiable);
        res.add_header("Content-Range", content_range("*", range_set.filesize));
        res.send();
        res.conn->close();
        return;
    case PartialContent:
        range_set.send_range_response(req, res);
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

void byte_range_set::send_range_response(request& req, response& res)
{
    std::string buf;
    int fd = open(req.path().c_str(), O_RDONLY);

    res.set_status_code(PartialContent);

    if (ranges.size() == 1) {
        auto& range = ranges.back();
        res.add_header("Content-Type", mime_type);
        res.add_header("Content-Range", content_range(range.to_str(), filesize));
        res.add_header("Content-Length", std::to_string(range.length()));
        res.send();
        res.conn->send_file(fd, range.first_byte_pos, range.length());
        res.conn->set_send_complete_handler([fd](const connection_ptr& conn){ close(fd); });
        return;
    }

    build_multipart_header(res);
    res.send();

    // Build and send each entity part in sequence.
    for (auto& range : ranges) {
        buf.append("--").append(boundary).append(CRLF);
        format_header(buf, "Content-Type", mime_type);
        format_header(buf, "Content-Range", content_range(range.to_str(), filesize));
        buf.append(CRLF);
        res.conn->send(buf);
        buf.clear();
        res.conn->send_file(fd, range.first_byte_pos, range.length());
        buf.append(CRLF);
    }
    buf.append("--").append(boundary).append("--").append(CRLF);
    res.conn->send(buf);
    res.conn->set_send_complete_handler([fd](const connection_ptr& conn){ close(fd); });
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

http_server::http_server(evloop *loop, inet_addr listen_addr)
    : server(loop, listen_addr)
{
    server.set_connection_handler([this](const connection_ptr& conn){
            context ctx;
            ctx.response.conn = conn.get();
            conn->set_context(std::move(ctx));
            conn->set_ttl(this->idle_time * 1000);
            });
    server.set_message_handler([this](const connection_ptr& conn, buffer& buf){
            this->message_handler(conn, buf);
            });
    set_base_dir(".");
    set_idle(30); // 30s by default
}

void http_server::set_base_dir(std::string_view dir)
{
    base_dir = dir;
    if (base_dir.back() == '/') base_dir.pop_back();
}

void http_server::set_parallel(unsigned n)
{
    if (n == 0) return;
    server.start_io_threads(n);
}

void http_server::set_idle(int secs)
{
    if (secs <= 0) return;
    idle_time = secs;
}

void http_server::generate_file_etag_by(std::string_view way)
{
    generate_file_etag_by_sha1 = (way == "sha1");
}

http_server& http_server::Get(std::string_view path, const ServerHandler handler)
{
    router[GET].emplace(path, std::move(handler));
    return *this;
}

http_server& http_server::Post(std::string_view path, const ServerHandler handler)
{
    router[POST].emplace(path, std::move(handler));
    return *this;
}

http_server& http_server::File(std::string_view path, const FileHandler handler)
{
    std::string file(base_dir);
    file += path;
    if (path == "/") file += "index.html";
    file_table.emplace(file, std::move(handler));
    return *this;
}

void http_server::start()
{
    server.set_nodelay(true);
    server.set_keepalive(true);
    server.start();
}

//====================================================
//=================== http_client ====================
//====================================================

http_request& http_request::set_url(std::string_view url)
{
    invalid_url = !uri.parse(url);
    return *this;
}

http_request& http_request::set_params(const Params& params)
{
    uri.params = params;
    return *this;
}

http_request& http_request::set_headers(const Headers& headers)
{
    this->headers = headers;
    return *this;
}

http_request& http_request::set_body(std::string_view body)
{
    this->body = body;
    return *this;
}

http_request& http_request::set_resolve_timeout(int ms)
{
    if (ms > 0) resolve_timeout = ms;
    return *this;
}

http_request& http_request::set_connection_timeout(int ms)
{
    if (ms > 0) connection_timeout = ms;
    return *this;
}

http_request& http_request::set_request_timeout(int ms)
{
    if (ms > 0) request_timeout = ms;
    return *this;
}

http_request& http_request::set_pending_timeout(int ms)
{
    if (ms > 0) pending_timeout = ms;
    return *this;
}

http_request& http_request::Get()
{
    method = "GET";
    return *this;
}

http_request& http_request::Post()
{
    method = "POST";
    return *this;
}

std::string& http_request::str()
{
    buf.clear();

    buf.append(method).append(1, SP);

    buf.append(uri.path);
    if (!uri.params.empty()) {
        buf.append("?");
    }
    for (auto& [k, v] : uri.params) {
        buf.append(uri.encode(k)).append("=").append(uri.encode(v)).append("&");
    }
    if (!uri.params.empty()) {
        buf.back() = SP;
    } else {
        buf.append(1, SP);
    }

    buf.append("HTTP/1.1").append(CRLF);

    headers["Host"] = uri.host;
    for (auto& [field, value] : headers)
        format_header(buf, field.key, value);

    buf.append(CRLF);

    return buf;
}

// Status-Line = HTTP-Version <SP> Status-Code <SP> Reason-Phrase <CRLF>
StatusCode http_response::parse_line(buffer& buf)
{
    int crlf = buf.find_crlf();
    if (crlf < 0) return Continue;

    auto p = buf.peek();
    auto end = buf.peek() + crlf;

    auto sp = std::find(p, end, SP);
    std::string_view version(p, (size_t)(sp - p));

    if (version == "HTTP/1.1") {
        http_version = HTTP_VERSION_1_1;
    } else if (version == "HTTP/1.0") {
        http_version = HTTP_VERSION_1_0;
    } else {
        return BadRequest;
    }

    p = sp + 1;
    sp = std::find(p, end, SP);

    status_code = util::svtoi({p, (size_t)(sp - p)}).value_or(0);
    if (status_code <= 0) return BadRequest;

    p = sp + 1;
    status_message.assign(p, end - p);

    buf.retrieve(crlf + 2);
    return Ok;
}

http_client::http_client()
{
    resolver = dns::resolver::get_resolver();
}

http_client::~http_client()
{
    router.clear();
    sender.join();
}

void http_client::set_max_conns_per_route(int conns)
{
    if (conns <= 0) return;
    max_conns_per_route = conns;
}

http_connection_pool *http_client::create_connection_pool(http_request& request)
{
    auto *pool = new http_connection_pool();

    pool->addrs = resolver->get_addr_list(request.uri.host, request.resolve_timeout);
    pool->pending = false;

    if (pool->addrs.empty()) {
        delete pool;
        return nullptr;
    } else {
        return pool;
    }
}

http_connection *http_connection_pool::create_connection()
{
    auto *http_conn = new http_connection();

    http_conn->pool = this;
    http_conn->create = true;
    http_conn->leased = true;
    http_conn->removing = false;

    leased.emplace_back(http_conn);

    return http_conn;
}

void http_connection_pool::lease_connection()
{
    leased.emplace_back(std::move(avail.back()));
    avail.pop_back();

    std::promise<http_response> response_promise;
    leased.back()->response_promise.swap(response_promise);
    leased.back()->leased = true;
}

void http_connection_pool::release_connection(http_connection *http_conn)
{
    assert(http_conn->leased);
    for (auto& conn : leased) {
        if (conn.get() == http_conn) {
            http_conn->leased = false;
            std::swap(conn, leased.back());
            avail.emplace_back(std::move(leased.back()));
            leased.pop_back();
            return;
        }
    }
}

void http_connection_pool::remove_connection(http_connection *http_conn)
{
    assert(!http_conn->removing);
    http_conn->removing = true;

    auto& conn_list = http_conn->leased ? leased : avail;

    for (auto& conn : conn_list) {
        if (conn.get() == http_conn) {
            // destruct http_conn -> ~client() -> remove_connection()
            std::swap(conn, conn_list.back());
            conn_list.pop_back();
            return;
        }
    }
}

bool http_connection_pool::wait_for(int pending_timeout)
{
    auto start = util::get_cur_time_ms();
    std::unique_lock<std::mutex> lk(pending_mutex);
    pending = true;
    while (pending) {
        pending_cv.wait_for(lk, std::chrono::milliseconds(pending_timeout));
        if (util::get_cur_time_ms() - start >= pending_timeout) {
            return false;
        }
    }
    return true;
}

void http_connection_pool::wakeup()
{
    std::lock_guard<std::mutex> lk(pending_mutex);

    if (pending) {
        pending = false;
        pending_cv.notify_one();
    }
}

int http_connection_pool::active_conns()
{
    return leased.size() + avail.size();
}

void http_connection::err_notify(ErrorCode err_code)
{
    response.err_code = err_code;
    response_promise.set_value(std::move(response));
    response.err_code = ErrorCode::None;
}

void http_client::send(http_connection *http_conn, http_request& request)
{
    auto& cli = http_conn->client;
    assert(cli->is_connected());
    cli->conn()->send(request.str());
    set_request_timeout_timer(http_conn, request.request_timeout);
}

// Add a new http connection to pool.
void http_client::add_connection(http_connection_pool *pool, http_request& request)
{
    auto *http_conn = pool->create_connection();

    // TODO: If failure, try other addrs
    inet_addr peer_addr(pool->addrs[0], request.uri.port);

#if defined (ANGEL_USE_OPENSSL)
    if (request.uri.scheme == "https") {
        http_conn->client.reset(new angel::ssl_client(sender.get_loop(), peer_addr));
    } else {
        http_conn->client.reset(new angel::client(sender.get_loop(), peer_addr));
    }
#else
    http_conn->client.reset(new angel::client(sender.get_loop(), peer_addr));
#endif

    auto& client = http_conn->client;

    client->set_connection_timeout_handler(request.connection_timeout, [this, http_conn](){
            this->connection_timeout_handler(http_conn);
            });
    client->set_connection_handler(
            [this, http_conn, request = request](const connection_ptr& conn) mutable {
            this->send(http_conn, request);
            });
    client->set_message_handler([this, http_conn](const connection_ptr& conn, buffer& buf){
            this->receive(http_conn, buf);
            });
    client->set_close_handler([this, http_conn](const connection_ptr& conn){
            this->connection_reset_by_peer(http_conn);
            });
}

void http_client::connection_timeout_handler(http_connection *http_conn)
{
    assert(http_conn->leased);
    http_conn->err_notify(ErrorCode::ConnectionTimeout);
    remove_connection(http_conn);
}

void http_client::set_request_timeout_timer(http_connection *http_conn, int request_timeout)
{
    http_conn->request_timeout_timer_id = sender.get_loop()->run_after(request_timeout,
            [this, http_conn]{
            http_conn->err_notify(ErrorCode::RequestTimeout);
            remove_connection(http_conn);
            });
}

void http_client::cancel_request_timeout_timer(http_connection *http_conn)
{
    if (http_conn->request_timeout_timer_id > 0) {
        sender.get_loop()->cancel_timer(http_conn->request_timeout_timer_id);
        http_conn->request_timeout_timer_id = 0;
    }
}

void http_client::connection_reset_by_peer(http_connection *http_conn)
{
    // To avoid calling remove_connection() repeatedly,
    // it can only be called when the server closes the connection,
    // because the http client will call remove_connection() externally.
    if (http_conn->removing) return;

    // If the connection is being leased, we need to notify the requester.
    if (http_conn->leased) {
        http_conn->err_notify(ErrorCode::ConnectionResetByPeer);
        // We must cancel request timeout timer before remove_connection(),
        // otherwise http_conn will become a hanging pointer.
        this->cancel_request_timeout_timer(http_conn);
    }
    this->remove_connection(http_conn);
}

void http_client::put_connection(http_connection *http_conn)
{
    std::lock_guard<std::mutex> lk(router_mutex);

    http_conn->pool->release_connection(http_conn);
}

void http_client::remove_connection(http_connection *http_conn)
{
    std::lock_guard<std::mutex> lk(router_mutex);

    http_conn->pool->remove_connection(http_conn);
}

static response_future err_future(ErrorCode err_code)
{
    std::promise<http_response> p;
    auto f = p.get_future();

    http_response res;
    res.err_code = err_code;
    p.set_value(std::move(res));

    return f;
}

response_future http_client::send(http_request& request)
{
    if (request.invalid_url || request.method.empty()) {
        return err_future(ErrorCode::InvalidRequest);
    }

    http_connection_pool *pool;
    auto route = util::concat(request.uri.host, ":", std::to_string(request.uri.port));
retry:
    // We hold the router_mutex during the whole process.
    router_mutex.lock();

    auto it = router.find(route);
    // Create a new http connection pool for route.
    if (it == router.end()) {
        if (!(pool = create_connection_pool(request))) {
            router_mutex.unlock();
            return err_future(ErrorCode::ResolveTimeoutOrNoAvailableAddr);
        }
        router.emplace(route, pool);
    } else {
        pool = it->second.get();
    }

    if (pool->avail.empty()) {
        if (pool->active_conns() < max_conns_per_route) {
            add_connection(pool, request);
        } else {
            // There is no connection available at the moment.
            router_mutex.unlock();
            if (!pool->wait_for(request.pending_timeout)) {
                return err_future(ErrorCode::PendingTimeout);
            }
            goto retry;
        }
    } else {
        pool->lease_connection();
    }

    auto *http_conn = pool->leased.back().get();

    auto f = http_conn->response_promise.get_future();

    // Request will be sent in connection_handler() for newly created connection.
    if (http_conn->create) {
        http_conn->create = false;
        http_conn->client->start();
    } else {
        send(http_conn, request);
    }

    router_mutex.unlock();

    return f;
}

void http_client::receive(http_connection *http_conn, buffer& buf)
{
    // The http connection is idle, ignoring all received messages.
    if (!http_conn->leased) {
        buf.retrieve_all();
        return;
    }
    // Parse Response, similar to http_server.
    StatusCode code;
    auto& res = http_conn->response;
    while (buf.readable() > 0) {
        switch (res.state) {
        case ParseLine:
            switch (code = res.parse_line(buf)) {
            case Ok:
                res.state = ParseHeader;
                break;
            case Continue:
                return;
            default:
                goto err;
            }
            break;
        case ParseHeader:
            switch (code = res.parse_header(buf)) {
            case Ok:
                switch (code = res.parse_body_length()) {
                case Ok:
                    res.state = ParseBody;
                    break;
                case Continue:
                    goto end;
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
            switch (code = res.parse_body(buf)) {
            case Ok:
                goto end;
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
    res.err_code = ErrorCode::InvalidResponse;
end:
    http_conn->response_promise.set_value(std::move(res));
    http_conn->response.state = ParseLine;
    put_connection(http_conn);

    cancel_request_timeout_timer(http_conn);
    // Try to wakeup pending requests.
    http_conn->pool->wakeup();
}

const char *http_response::err_str()
{
    switch (err_code) {
    case ErrorCode::InvalidRequest: return "Invalid Request";
    case ErrorCode::InvalidResponse: return "Invalid Response";
    case ErrorCode::ResolveTimeoutOrNoAvailableAddr: return "Resolve Timeout Or No Available Addr";
    case ErrorCode::ConnectionTimeout: return "Connection Timeout";
    case ErrorCode::RequestTimeout: return "Request Timeout";
    case ErrorCode::PendingTimeout: return "Pending Timeout";
    case ErrorCode::ConnectionResetByPeer: return "Connection Reset By Peer";
    case ErrorCode::None: return "None";
    }
}

}
}
