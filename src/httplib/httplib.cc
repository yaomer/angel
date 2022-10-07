#include <angel/httplib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

namespace angel {
namespace httplib {

// [method url version\r\n]
bool request::parse_line(buffer& buf, int crlf)
{
    const char *p = buf.peek();
    const char *ep = buf.peek() + crlf;
    const char *next = std::find(p, ep, ' ');
    method.assign(p, next);
    std::transform(method.begin(), method.end(), method.begin(), ::toupper);
    p = next + 1;
    next = std::find(p, ep, '?');
    if (next == ep) {
        next = std::find(p, ep, ' ');
        url.assign(p, next);
    } else {
        url.assign(p, next);
        while (true) {
            p = next + 1;
            next = std::find(p, ep, '&');
            if (next != ep) {
                args.emplace_back(p, next);
            } else {
                next = std::find(p, ep, ' ');
                args.emplace_back(p, next);
                break;
            }
        }
    }
    version.assign(next + 1, ep);
    return true;
}

bool request::parse_header(buffer& buf, int crlf)
{
    const char *p = buf.peek();
    if (buf.starts_with("\r\n")) {
        return true;
    }
    if (buf.starts_with_case("Host: ")) {
        host.assign(p + 6, p + crlf);
    } else if (buf.starts_with_case("Connection: ")) {
        connection.assign(p + 12, p + crlf);
    } else {
        ; // TODO: Other Options
    }
    return false;
}

std::string& response::str()
{
    buf.append("HTTP/1.1 ").append(std::to_string(status_code))
       .append(" ").append(status_message).append("\r\n");

    if (!body.empty()) {
        add_header("Content-Length", std::to_string(body.size()));
    }

    for (auto& [field, value] : headers) {
        buf.append(field).append(": ").append(value).append("\r\n");
    }
    buf.append("\r\n");

    if (!body.empty()) buf.append(body);

    return buf;
}

void response::clear()
{
    body.clear();
    headers.clear();
    buf.clear();
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

void HttpServer::start()
{
    set_base_dir(".");
    server.start();
}

void HttpServer::message_handler(const connection_ptr& conn, buffer& buf)
{
    if (!conn->is_connected()) return;
    log_info(buf.c_str());
    auto& ctx = std::any_cast<context&>(conn->get_context());
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (ctx.state) {
        case ParseLine:
            ctx.request.parse_line(buf, crlf);
            ctx.state = ParseHeader;
            break;
        case ParseHeader:
            if (ctx.request.parse_header(buf, crlf))
                process_request(conn);
            break;
        }
        buf.retrieve(crlf + 2);
    }
}

static bool is_file(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    return S_ISREG(st.st_mode);
}

static std::string read_file(const std::string& path)
{
    std::string res;
    int fd = open(path.c_str(), O_RDONLY);
    assert(fd > 0);
    buffer buf;
    while (buf.read_fd(fd) > 0)
        res.append(buf.peek(), buf.readable());
    close(fd);
    return res;
}

void HttpServer::process_request(const connection_ptr& conn)
{
    auto& ctx = std::any_cast<context&>(conn->get_context());
    auto& req = ctx.request;
    auto& res = ctx.response;

    res.add_header("Server", "angel");
    if (req.method == "GET") {
        auto& table = router["GET"];
        auto it = table.find(req.url);
        if (it != table.end()) {
            it->second(req, res);
        } else {
            std::string path(base_dir + req.url);
            if (is_file(path)) {
                res.set_status_code(Ok);
                res.set_status_message("OK");
                res.set_body(read_file(path));
            } else {
                res.set_status_code(NotFound);
                res.set_status_message("Not Found");
            }
        }
    } else {
        res.set_status_code(NotImplemented);
        res.set_status_message("Method Not Implemented");
    }
    conn->send(res.str());
    res.clear();
    ctx.state = ParseLine;

    if (strcasecmp(req.connection.c_str(), "Close") == 0) {
        conn->close();
    } else if (strcasecmp(req.connection.c_str(), "Keep-Alive") != 0) {
        // Error
    }
}

HttpServer& HttpServer::Get(std::string_view path, const ServerHandler handler)
{
    router["GET"].emplace(path, std::move(handler));
    return *this;
}

}
}
