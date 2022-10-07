#include <angel/httplib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace angel {
namespace httplib {

static bool equal_case(std::string_view s1, std::string_view s2)
{
    return s1.size() == s2.size() && strncasecmp(s1.begin(), s2.begin(), s1.size()) == 0;
}

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

// Date: Wed, 15 Nov 1995 06:25:24 GMT
std::string format_date()
{
    std::ostringstream oss;
    auto tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    oss << std::put_time(std::gmtime(&tm), "%a, %d %b %Y %T GMT");
    return oss.str();
}

std::string& response::str()
{
    buf.append("HTTP/1.1 ")
       .append(std::to_string(status_code)).append(" ")
       .append(status_message).append("\r\n");

    add_header("Date", format_date());
    if (!content.empty()) {
        add_header("Content-Length", std::to_string(content.size()));
    }

    for (auto& [field, value] : headers) {
        buf.append(field).append(": ").append(value).append("\r\n");
    }
    buf.append("\r\n");

    if (!content.empty()) buf.append(content);

    return buf;
}

void response::clear()
{
    content.clear();
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
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (ctx.state) {
        case ParseLine:
            ctx.request.parse_line(buf, crlf);
            ctx.state = ParseHeader;
            break;
        case ParseHeader:
            if (ctx.request.parse_header(buf, crlf)) {
                process_request(conn);
            }
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
            if (req.url == "/") {
                path += "index.html";
                res.add_header("Content-Type", "text/html");
            }
            if (is_file(path)) {
                res.set_status_code(Ok);
                res.set_status_message("OK");
                res.set_content(read_file(path));
            } else {
                res.set_status_code(NotFound);
                res.set_status_message("Not Found");
            }
        }
    } else {
        res.set_status_code(NotImplemented);
        res.set_status_message("Method Not Implemented");
    }

    bool keep_alive = equal_case(req.connection, "keep-alive");
    res.add_header("Connection", keep_alive ? "keep-alive" : "close");

    conn->send(res.str());
    res.clear();
    ctx.state = ParseLine;

    if (!keep_alive) {
        conn->close();
    }
}

HttpServer& HttpServer::Get(std::string_view path, const ServerHandler handler)
{
    router["GET"].emplace(path, std::move(handler));
    return *this;
}

}
}
