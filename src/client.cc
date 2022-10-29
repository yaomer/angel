#include <angel/client.h>

#include <angel/logger.h>

#include "connector.h"

namespace angel {

client::client(evloop *loop, inet_addr peer_addr, client_options ops)
    : loop(loop), ops(ops), peer_addr(peer_addr),
    connection_timeout_timer_id(0),
    high_water_mark(0)
{
}

client::~client()
{
    close_connection();
}

void client::new_connection(int fd)
{
    log_info("client(fd=%d) connected to host (%s)", fd, peer_addr.to_host());
    cli_conn = connection_ptr(new connection(1, loop, fd));
    cli_conn->set_connection_handler(connection_handler);
    cli_conn->set_message_handler(message_handler);
    cli_conn->set_high_water_mark_handler(high_water_mark, high_water_mark_handler);
    cli_conn->set_close_handler([this](const connection_ptr& conn){
            this->close_connection();
            if (this->ops.is_reconnect) this->start();
            });
    establish();
}

void client::establish()
{
    loop->run_in_loop([conn = cli_conn]{ conn->establish(); });
    cancel_connection_timeout_timer();
}

void client::close_connection()
{
    close_connection(nullptr);
}

void client::close_connection(functor f)
{
    if (!cli_conn) return;
    if (cli_conn->is_closed()) return;
    if (close_handler) close_handler(cli_conn);
    cli_conn->set_state(connection::state::closed);
    loop->remove_channel(cli_conn->channel);
    // should close(fd) after remove_channel()
    loop->run_in_loop([conn = cli_conn, f = std::move(f)](){ if(f) f(); });
    cancel_connection_timeout_timer();
    if (ops.is_quit_loop) loop->quit();
}

void client::add_connection_timeout_timer()
{
    if (connection_timeout_handler) {
        connection_timeout_timer_id = loop->run_after(connection_timeout, [this]{
                if (this->is_connected()) return;
                // Avoid the context in which it is executing is changed,
                // when call set_connection_timeout_handler() in connection_timeout_handler()
                auto handler = this->connection_timeout_handler;
                handler();
                });
    }
}

void client::cancel_connection_timeout_timer()
{
    if (connection_timeout_timer_id > 0) {
        loop->cancel_timer(connection_timeout_timer_id);
        connection_timeout_timer_id = 0;
    }
}

void client::start()
{
    add_connection_timeout_timer();
    connector.reset(new connector_t(loop, peer_addr));
    connector->new_connection_handler = [this](int fd){ this->new_connection(fd); };
    connector->retry_interval = ops.retry_interval_ms;
    connector->protocol = ops.protocol;
    connector->connect();
}

void client::send(std::string_view data)
{
    cli_conn->send(data);
}

void client::restart()
{
    close_connection();
    start();
}

void client::restart(inet_addr peer_addr)
{
    this->peer_addr = peer_addr;
    restart();
}

bool client::is_connected()
{
    return connector->is_connected() && cli_conn;
}

void client::set_connection_handler(connection_handler_t handler)
{
    connection_handler = std::move(handler);
}

void client::set_message_handler(message_handler_t handler)
{
    message_handler = std::move(handler);
}

void client::set_close_handler(close_handler_t handler)
{
    close_handler = std::move(handler);
}

void client::set_connection_timeout_handler(int timeout_ms, connection_timeout_handler_t handler)
{
    if (timeout_ms <= 0) return;
    connection_timeout = timeout_ms;
    connection_timeout_handler = std::move(handler);
}

void client::set_high_water_mark_handler(size_t size, high_water_mark_handler_t handler)
{
    high_water_mark = size;
    high_water_mark_handler = std::move(handler);
}

void client::start_task_threads(size_t thread_nums, enum thread_pool::policy policy)
{
    if (thread_nums > 0) {
        task_thread_pool.reset(new thread_pool(policy, thread_nums));
    } else {
        task_thread_pool.reset(new thread_pool(policy));
    }
}

void client::executor(const task_callback_t task)
{
    if (task_thread_pool) {
        task_thread_pool->executor(task);
    }
}

}
