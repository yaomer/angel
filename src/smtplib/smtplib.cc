//
// Async SMTP Client
// See https://www.rfc-editor.org/rfc/rfc5321.html
//

#include <angel/smtplib.h>

#include <angel/client.h>
#include <angel/util.h>
#include <angel/sockops.h>
#include <angel/logger.h>

namespace angel {
namespace smtplib {

smtp::smtp() : id(1)
{
    sender.wait_loop();
    resolver = dns::resolver::get_resolver();
}

smtp::~smtp()
{
}

enum State {
    INIT, EHLO, AUTH, MAIL, RCPT, DATA, QUIT, CLOSE
};

// The reason why we need to inherit `enable_shared_from_this` is
// because there are timed events with deferred execution.
struct send_task : public std::enable_shared_from_this<send_task> {
    size_t id;
    std::string host;
    int port;
    std::queue<std::string> try_addrs;
    std::unique_ptr<client> cli;
    int state = INIT;
    size_t timer_id = 0;
    size_t rcpt_index = 0;
    bool need_auth = false;
    std::string username;
    std::string password;
    std::string sender;
    std::vector<std::string> receivers;
    std::string mail_data;
    std::promise<result> res_promise;
    smtp *smtp;

    void start();
    void set_try_addrs();
    void set_result(bool is_ok, std::string_view err = {});
    void send_mail(const connection_ptr& conn, buffer& buf);
    // Command Execution Sequence
    void ehlo();
    void auth();
    void do_auth(std::string_view arg);
    void mail();
    void rcpt();
    void data();
    void do_data();
    void quit();
};

result_future smtp::send_mail(std::string_view host, int port,
                              std::string_view username, std::string_view password,
                              std::string_view sender, // sender mailbox
                              const std::vector<std::string>& receivers, // receiver mailbox list
                              std::string_view data // mail data
                              )
{
    auto *task = new send_task();

    task->id        = id++;
    task->host      = host;
    task->port      = port;
    task->username  = username;
    task->password  = password;
    task->sender    = sender;
    task->receivers = receivers;
    task->mail_data = data;
    task->smtp      = this;

    task->set_try_addrs();

    auto f = task->res_promise.get_future();
    {
        std::lock_guard<std::mutex> lk(task_map_mutex);
        task_map.emplace(task->id, task);
    }
    task->start();

    return f;
}

void send_task::set_try_addrs()
{
    auto addr_list = smtp->resolver->get_addr_list(host);
    for (auto& addr : addr_list) {
        try_addrs.emplace(std::move(addr));
    }
}

void send_task::start()
{
    if (try_addrs.empty()) {
        std::string err = "No " + host + " address available";
        log_error("(smtplib) %s", err.c_str());
        set_result(false, err);
        return;
    }

    // Try another addr
    inet_addr conn_addr(try_addrs.front().c_str(), port);
    try_addrs.pop();

    cli.reset(new client(smtp->sender.get_loop(), conn_addr));
    cli->set_message_handler([this](const connection_ptr& conn, buffer& buf){
            this->send_mail(conn, buf);
            });
    cli->start();

    // If can't connect to the addr after 30s, try another addr
    timer_id = smtp->sender.get_loop()->run_after(1000 * 30, [task = shared_from_this()]{
            if (task->cli->is_connected()) return;
            log_warn("(smtplib) can't connect to (%s), try another...", task->cli->get_peer_addr().to_host());
            task->start();
            });
}

void send_task::set_result(bool is_ok, std::string_view err)
{
    if (timer_id > 0) {
        smtp->sender.get_loop()->cancel_timer(timer_id);
    }

    result res;
    res.is_ok = is_ok;
    res.err = err;

    // Notify the caller
    res_promise.set_value(std::move(res));
    {
        std::lock_guard<std::mutex> lk(smtp->task_map_mutex);
        smtp->task_map.erase(id);
    }
}

void send_task::ehlo()
{
    cli->conn()->format_send("EHLO %s\r\n", sockops::get_host_name());
    log_debug("(smtplib) C: EHLO %s", sockops::get_host_name());
    state = EHLO;
}

void send_task::auth()
{
    cli->conn()->send("AUTH LOGIN\r\n");
    log_debug("(smtplib) C: AUTH LOGIN");
    state = AUTH;
}

void send_task::do_auth(std::string_view arg)
{
    auto res = util::base64_decode(arg);

    if (strcasecmp(res.c_str(), "username:") == 0) {
        auto s = util::base64_encode(username);
        cli->conn()->send(s.append("\r\n"));
        log_debug("(smtplib) C: %s", s.c_str());
    } else if (strcasecmp(res.c_str(), "password:") == 0) {
        auto s = util::base64_encode(password);
        cli->conn()->send(s.append("\r\n"));
        log_debug("(smtplib) C: %s", s.c_str());
    }
}

void send_task::mail()
{
    cli->conn()->format_send("MAIL FROM:<%s>\r\n", sender.c_str());
    log_debug("(smtplib) C: MAIL FROM:<%s>", sender.c_str());
    state = MAIL;
}

void send_task::rcpt()
{
    cli->conn()->format_send("RCPT TO:<%s>\r\n", receivers[rcpt_index++].c_str());
    log_debug("(smtplib) C: RCPT TO:<%s>", receivers[rcpt_index-1].c_str());
    if (rcpt_index >= receivers.size()) {
        rcpt_index = 0;
        state = RCPT;
    }
}

void send_task::data()
{
    cli->conn()->send("DATA\r\n");
    log_debug("(smtplib) C: DATA");
    state = DATA;
}

void send_task::do_data()
{
    cli->conn()->send(mail_data.append("\r\n.\r\n"));
    log_debug("(smtplib) C: %s", mail_data.c_str());
    state = QUIT;
}

void send_task::quit()
{
    cli->conn()->send("QUIT\r\n");
    log_debug("(smtplib) C: QUIT");
    state = CLOSE;
}

void send_task::send_mail(const connection_ptr& conn, buffer& buf)
{
    log_debug("(smtplib) S(%s): %s", conn->get_peer_addr().to_host(), buf.c_str());
    while (buf.readable() > 0) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (state) {
        case INIT:
            // 220 <domain> Service ready
            if (buf.starts_with("220 ")) ehlo();
            else goto error;
            break;
        case EHLO:
            // 250-smtp.example.com
            // 250-PIPELINING
            // 250-SIZE 73400320
            // 250-STARTTLS
            // 250-AUTH LOGIN PLAIN XOAUTH XOAUTH2
            // 250-AUTH=LOGIN
            // 250 8BITMIME
            if (buf.starts_with("250-")) {
                if (buf.starts_with_case("250-AUTH")) {
                    need_auth = true;
                }
            } else if (buf.starts_with("250 ")) {
                if (need_auth) auth();
                else mail();
            } else {
                goto error;
            }
            break;
        case AUTH:
            // 334 Username:
            // 334 Password:
            if (buf.starts_with("334 ")) {
                std::string_view arg(buf.peek() + 4, crlf - 4);
                do_auth(arg);
            // 235 Authentication successful
            } else if (buf.starts_with("235 ")) {
                mail();
            } else {
                goto error;
            }
            break;
        case MAIL:
            // 250 OK
            if (buf.starts_with("250 ")) rcpt();
            else goto error;
            break;
        case RCPT:
            // 250 OK
            if (buf.starts_with("250 ")) data();
            else goto error;
            break;
        case DATA:
            // 354 Start mail input; end with <CRLF>.<CRLF>
            if (buf.starts_with("354 ")) do_data();
            else goto error;
            break;
        case QUIT:
            // 250 Mail OK queued as.
            if (buf.starts_with("250 ")) quit();
            else goto error;
            break;
        case CLOSE:
            // 221 <domain> Service closing transmission channel
            if (buf.starts_with("221 ")) {
                set_result(true);
                return;
            }
            goto error;
        }
        buf.retrieve(crlf + 2);
    }
    return;
error:
    log_error("(smtplib) S(%s): %s", conn->get_peer_addr().to_host(), buf.c_str());
    set_result(false, buf.c_str());
}

}
}
