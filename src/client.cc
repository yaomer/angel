#include "client.h"
#include "logger.h"

using namespace angel;

client::client(evloop *loop, inet_addr peer_addr)
    : loop(loop),
    connector(new connector_t(
                loop, peer_addr, [this](int fd){ this->new_connection(fd); })),
    flag(flags::exit_loop),
    high_water_mark(0)
{
}

client::~client()
{
    close_connection(cli_conn);
}

void client::new_connection(int fd)
{
    cli_conn = connection_ptr(new connection(1, loop, fd));
    cli_conn->set_state(connection::state::connected);
    cli_conn->set_connection_handler(connection_handler);
    cli_conn->set_message_handler(message_handler);
    cli_conn->set_write_complete_handler(write_complete_handler);
    cli_conn->set_high_water_mark_handler(high_water_mark, high_water_mark_handler);
    cli_conn->set_close_handler([this](const connection_ptr& conn){
            this->close_connection(conn);
            });
    loop->run_in_loop([conn = cli_conn]{
            conn->establish();
            });
}

void client::start()
{
    connector->connect();
}

void client::close_connection(const connection_ptr& conn)
{
    if (is_enum_true(flag & flags::disconnect))
        return;
    if (is_connected()) {
        if (close_handler) close_handler(conn);
        conn->set_state(connection::state::closed);
        loop->remove_channel(conn->get_channel());
    }
    cli_conn.reset();
    if (is_enum_true(flag & flags::exit_loop))
        loop->quit();
    flag |= flags::disconnect;
}
