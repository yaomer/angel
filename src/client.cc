#include "connector.h"
#include "client.h"
#include "logger.h"

namespace angel {

client::client(evloop *loop, inet_addr peer_addr, client_options ops)
    : loop(loop), ops(ops), peer_addr(peer_addr),
    high_water_mark(0)
{
}

client::~client()
{
    close_connection(cli_conn);
}

bool client::is_connected()
{
    return connector->is_connected() && cli_conn;
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
            if (this->ops.is_reconnect) this->start();
            });
    loop->run_in_loop([conn = cli_conn]{
            conn->establish();
            });
}

void client::start()
{
    auto handler = [this](int fd){ this->new_connection(fd); };
    connector.reset(new connector_t(loop, peer_addr, handler, ops.retry_interval_ms));
    connector->connect();
}

void client::restart(inet_addr peer_addr)
{
    this->peer_addr = peer_addr;
    close_connection(cli_conn);
    start();
}

void client::close_connection(const connection_ptr& conn)
{
    if (!conn) return;
    if (conn->is_closed()) return;
    conn->set_state(connection::state::closed);
    loop->remove_channel(conn->get_channel());
    if (close_handler) close_handler(conn);
    cli_conn.reset();
    if (ops.is_quit_loop) loop->quit();
}

void client::set_task_thread_nums(size_t thread_nums)
{
    if (thread_nums > 0)
        task_thread_pool.reset(
                new thread_pool(thread_pool::policy::fixed, thread_nums));
    else
        task_thread_pool.reset(
                new thread_pool(thread_pool::policy::fixed));

}
void client::executor(const task_callback_t task)
{
    if (!task_thread_pool) {
        log_error("task_thread_pool is null");
        return;
    }
    task_thread_pool->executor(task);
}

}
