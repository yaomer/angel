#include <angel/server.h>
#include <stdio.h>

// 用telnet充当client，可以多人匿名聊天

const char *help = {
    "usage:\n"
    "-------------------------------------------------------------------------\n"
    "|id num [msg]| send [msg] to [id num]                                   |\n"
    "|grp [msg] | broadcast [msg] to all clients                             |\n"
    "|[msg]     | if ([last-id] is null) echo [msg]; else id [last-id] [msg] |\n"
    "|help      | show help msg                                              |\n"
    "|ls        | list all online users                                      |\n"
    "-------------------------------------------------------------------------\n"
};

angel::server *g_serv = nullptr;

void send_online_users(const angel::connection_ptr& conn)
{
    conn->send("> Online Users: ");
    g_serv->for_each([c = conn](const angel::connection_ptr& conn){
            char buf[32];
            snprintf(buf, sizeof(buf), "%zu ", conn->id());
            c->send(buf);
            });
    conn->send("\n");
}

void connection_handler(const angel::connection_ptr& conn)
{
    size_t clients = g_serv->get_all_connections().size();
    conn->format_send("> Welcome to chat-room, your chat-id is %zu, "
                      "there %s %zu people is online\n",
                      conn->id(),
                      clients > 1 ? "are" : "is",
                      clients);
    g_serv->for_each([c = conn](const angel::connection_ptr& conn){
            if (c->id() != conn->id())
                conn->format_send("> A new user [id %zu] is coming\n", c->id());
            });
    send_online_users(conn);
}

void close_handler(const angel::connection_ptr& conn)
{
    g_serv->for_each([c = conn](const angel::connection_ptr& conn){
            if (c->id() != conn->id())
                conn->format_send("> User [id %zu] is offline\n", c->id());
            });
}

void message_handler(const angel::connection_ptr& conn, angel::buffer& buf)
{
    char sbuf[256];
    snprintf(sbuf, sizeof(sbuf), "> [id %zu]: ", conn->id());
    // buf.readable() >= strlen("\n")
    while (buf.readable() >= 1) {
        int lf = buf.find_lf();
        if (lf >= 0) {
            if (buf.strcmp("help")) {
                conn->send(help);
                buf.retrieve(lf + 1);
            } else if (buf.strcmp("ls")) {
                send_online_users(conn);
                buf.retrieve(lf + 1);
            } else if (buf.strcmp("grp ")) {
                // broadcast msg to all clients
                buf.retrieve(4);
                g_serv->for_each([&sbuf, &buf, lf](const angel::connection_ptr& conn){
                        conn->send(sbuf);
                        conn->send(buf.c_str(), lf + 1 - 4);
                        });
                buf.retrieve(lf + 1 - 4);
            } else if (buf.strcmp("id ")) {
                // send msg to id
                buf.retrieve(3);
                int i = 0;
                while (isalnum(buf[i]))
                    i++;
                buf[i] = '\0';
                size_t id = atol(buf.c_str());
                conn->set_context(id);
                buf.retrieve(i);
                auto it = g_serv->get_connection(id);
                if (it) {
                    it->send(sbuf);
                    it->send(buf.c_str(), lf + 1 - (i + 3));
                } else {
                    conn->format_send("User [id %zu] is gone\n", id);
                }
                buf.retrieve(lf + 1 - (i + 3));
            } else {
                if (!conn->get_context().has_value()) {
                    // echo msg
                    conn->send(sbuf);
                    conn->send(buf.c_str(), lf + 1);
                    buf.retrieve(lf + 1);
                    continue;
                }
                // continue chat with id
                size_t id = std::any_cast<size_t>(conn->get_context());
                auto it = g_serv->get_connection(id);
                if (it) {
                    it->send(sbuf);
                    it->send(buf.c_str(), lf + 1);
                } else {
                    conn->format_send("User [id %zu] is gone\n", id);
                }
                buf.retrieve(lf + 1);
            }
        } else
            break;
    }
}

int main()
{
    angel::server::daemon();
    angel::evloop loop;
    angel::server serv(&loop, angel::inet_addr(8000));
    g_serv = &serv;
    serv.set_connection_handler(connection_handler);
    serv.set_message_handler(message_handler);
    serv.set_close_handler(close_handler);
    serv.start();
    loop.run();
}
