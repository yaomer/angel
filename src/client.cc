#include <angel/client.h>

#include <angel/util.h>
#include <angel/logger.h>

#include "connector.h"

namespace angel {

client::client(evloop *loop, inet_addr peer_addr, client_options ops)
    : loop(loop), ops(ops), peer_addr(peer_addr),
    connection_timeout_timer_id(0),
    high_water_mark(0)
{
    Assert(loop);
}

client::~client()
{
    active_shutdown();
}

size_t client::get_next_id()
{
    // All clients share conn_id.
    static std::atomic_size_t conn_id = 1;
    return conn_id.fetch_add(1, std::memory_order_relaxed);
}

connection_ptr client::create_connection(channel *chl)
{
    return std::make_shared<connection>(get_next_id(), chl);
}

void client::establish(channel *chl)
{
    Assert(loop->is_io_loop_thread());
    cli_conn = create_connection(chl);
    log_info("client(id=%zu, fd=%d) connected to host (%s)", cli_conn->id(), chl->fd(), peer_addr.to_host());
    cli_conn->set_message_handler(message_handler);
    cli_conn->set_high_water_mark_handler(high_water_mark, high_water_mark_handler);
    cli_conn->set_close_handler([this](const connection_ptr& conn){
            this->shutdown(conn);
            });
    cancel_connection_timeout_timer();
    connected = true;
    if (connection_handler) connection_handler(cli_conn);
}

void client::shutdown(const connection_ptr& conn)
{
    connected = false;
    cancel_connection_timeout_timer();
    if (ops.is_quit_loop) {
        loop->quit();
    } else if (ops.is_reconnect && conn->is_reset_by_peer()) {
        restart();
    }
    // User may delete this(client self) directly in close_handler().
    // So we finally call close_handler.
    //
    // To be on the safe side, we can't access any non-static members
    // of the client after this.
    if (close_handler) close_handler(conn);
}

void client::active_shutdown()
{
    connected = false;
    if (connector) {
        connector.reset();
    }
    if (cli_conn) {
        ops.is_reconnect = false;
        cli_conn->close_wait();
    }
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
    }
}

void client::start()
{
    add_connection_timeout_timer();
    connector.reset(new connector_t(loop, peer_addr));
    connector->onconnect = [this](channel *chl){ this->establish(chl); };
    connector->onfail    = [this]{ if (connection_failure_handler) connection_failure_handler(); };
    connector->protocol       = ops.protocol;
    connector->keep_reconnect = ops.keep_reconnect;
    connector->retry_interval = ops.retry_interval_ms;
    connector->connect();
}

void client::restart()
{
    active_shutdown();
    start();
}

void client::restart(inet_addr peer_addr)
{
    this->peer_addr = peer_addr;
    restart();
}

bool client::is_connected() const
{
    return connected;
}

const connection_ptr& client::conn() const
{
    Assert(is_connected());
    return cli_conn;
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

void client::set_connection_failure_handler(connection_failure_handler_t handler)
{
    connection_failure_handler = std::move(handler);
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
