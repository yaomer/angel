//
// SMTP Server
// See https://www.rfc-editor.org/rfc/rfc5321.html
//

#include "smtp-server.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <uuid/uuid.h>

static const char *mail_queue = "mail_queue";
static const char *mail_sent = "mail_sent";
static const char *tmpdir = "tmp";

static std::string generate_id()
{
    uuid_t uu;
    uuid_string_t out;
    uuid_generate(uu);
    uuid_unparse_lower(uu, out);
    return out;
}

static std::string get_mail_filename(std::string_view username)
{
    std::string filename(mail_queue);
    return filename.append("/").append(username).append("-").append(generate_id()).append(".mail");
}

smtp_server::smtp_server(angel::evloop *loop, angel::inet_addr listen_addr)
    : smtp(loop, listen_addr)
{
    mkdir(mail_queue, 0744);
    mkdir(mail_sent, 0744);
    mkdir(tmpdir, 0744);
    smtp.set_connection_handler([listen_addr](const angel::connection_ptr& conn){
            conn->format_send("220 %s Simple Mail Transfer Service Ready\r\n", listen_addr.to_host());
            conn->set_context(smtp_context());
            });
    smtp.set_message_handler([this](const angel::connection_ptr& conn, angel::buffer& buf){
            this->receive_mail(conn, buf);
            });
    smtp.start();
}

#define switch_case(state, ok, error) \
    switch (state) { \
    case Ok: conn->send(ok); break; \
    case Error: conn->send(error); conn->close(); return; \
    case NotEnough: return; \
    }

void smtp_server::receive_mail(const angel::connection_ptr& conn, angel::buffer& buf)
{
    smtp_context& c = std::any_cast<smtp_context&>(conn->get_context());
    while (buf.readable() > 0) {
        switch (c.parse_quit(buf)) {
        case Ok:
            conn->format_send("221 Service closing transmission channel\r\n");
            conn->close();
            return;
        case NotEnough: return;
        }
        switch (c.cmd) {
        case INIT:
            switch_case(c.parse_ehlo(buf),
                        "250 HELP\r\n",
                        "503 Bad sequence of commands\r\n");
            break;
        case EHLO:
            switch_case(c.parse_mail(buf),
                        "250 OK\r\n",
                        "503 Bad sequence of commands\r\n");
            break;
        case MAIL:
            switch_case(c.parse_rcpt(buf),
                        "250 OK\r\n",
                        "503 Bad sequence of commands\r\n");
            break;
        case RCPT:
            switch (c.parse_rcpt(buf)) {
            case Ok: conn->send("250 OK\r\n"); break;
            case Error:
                switch_case(c.parse_data(buf),
                            "354 Start mail input; end with <CRLF>.<CRLF>\r\n",
                            "503 Bad sequence of commands\r\n");
                break;
            case NotEnough: return;
            }
            break;
        case DATA:
            if (c.recv_data(buf) == NotEnough) return;
            conn->send("250 OK\r\n");
            break;
        case QUIT:
            conn->close();
            return;
        }
    }
}

int smtp_context::parse_ehlo(angel::buffer& buf)
{
    char *p = buf.peek();
    int crlf = buf.find_crlf();
    if (crlf < 0) return NotEnough;
    if (!buf.starts_with("EHLO")) return Error;
    origin.assign(p + 5, p + crlf);
    cmd = EHLO;
    buf.retrieve(crlf + 2);
    return Ok;
}

int smtp_context::parse_mail(angel::buffer& buf)
{
    char *p = buf.peek();
    int crlf = buf.find_crlf();
    if (crlf < 0) return NotEnough;
    const char *end = p + crlf;
    if (!buf.starts_with("MAIL FROM:<")) return Error;
    p += 11;
    int s = buf.find(p, ">");
    if (s == -1) return Error;
    mail_from.assign(p, s);
    cmd = MAIL;
    buf.retrieve(crlf + 2);
    return Ok;
}

int smtp_context::parse_rcpt(angel::buffer& buf)
{
    char *p = buf.peek();
    int crlf = buf.find_crlf();
    if (crlf < 0) return NotEnough;
    const char *end = p + crlf;
    if (!buf.starts_with("RCPT TO:<")) return Error;
    p += 9;
    int s = buf.find(p, ">");
    if (s == -1) return Error;
    mail_to.emplace_back(p, s);
    cmd = RCPT;
    buf.retrieve(crlf + 2);
    return Ok;
}

int smtp_context::parse_data(angel::buffer& buf)
{
    int crlf = buf.find_crlf();
    if (crlf < 0) return NotEnough;
    if (!buf.starts_with("DATA")) return Error;
    cmd = DATA;
    buf.retrieve(crlf + 2);
    return Ok;
}

int smtp_context::recv_data(angel::buffer& buf)
{
    char *p = buf.peek();
    int crlf = buf.find("\r\n.\r\n");
    if (crlf < 0) return NotEnough;
    save_mail(p, crlf);
    cmd = QUIT;
    buf.retrieve(crlf + 5);
    return Ok;
}

int smtp_context::parse_quit(angel::buffer& buf)
{
    int crlf = buf.find_crlf();
    if (crlf < 0) return NotEnough;
    if (!buf.starts_with("QUIT")) return Error;
    buf.retrieve(crlf + 2);
    return Ok;
}

void smtp_context::save_mail(const char *data, size_t len)
{
    char tmpfile[] = "tmp/tmp.XXXXXX";
    mktemp(tmpfile);
    int fd = open(tmpfile, O_RDWR | O_APPEND | O_CREAT, 0644);
    assert(fd >= 0);
    uint32_t receiver = mail_to.size();
    write(fd, &receiver, sizeof(receiver));
    for (auto& name : mail_to) {
        uint16_t len = name.size();
        write(fd, &len, sizeof(len));
        write(fd, name.data(), name.size());
    }
    write(fd, "\n", 1);
    write(fd, data, len);
    close(fd);

    auto mail_filename = get_mail_filename(mail_from);
    rename(tmpfile, mail_filename.c_str());
}

int main()
{
    angel::evloop loop;
    smtp_server smtp(&loop, angel::inet_addr(8000));
    loop.run();
}
