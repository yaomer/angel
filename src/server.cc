#include <angel/server.h>

#include <stdlib.h>

#include <angel/signal.h>

#include "listener.h"
#include "evloop_group.h"

namespace angel {

server::server(evloop *loop, inet_addr listen_addr)
    : loop(loop),
    listener(new listener_t(loop, listen_addr)),
    conn_id(1),
    high_water_mark(0)
{
    // if (is_set_cpu_affinity)
        // util::set_thread_affinity(pthread_self(), 0);
}

server::~server()
{
}

inet_addr& server::listen_addr()
{
    return listener->addr();
}

evloop *server::get_next_loop()
{
    if (io_loop_group && io_loop_group->size() > 0) {
        return io_loop_group->get_next_loop();
    } else {
        return loop;
    }
}

connection_ptr server::create_connection(int fd)
{
    size_t id = conn_id++;
    evloop *io_loop = get_next_loop();
    return std::make_shared<connection>(id, io_loop, fd);
}

void server::new_connection(int fd)
{
    connection_ptr conn(create_connection(fd));
    conn->set_connection_handler(connection_handler);
    conn->set_message_handler(message_handler);
    conn->set_high_water_mark_handler(high_water_mark, high_water_mark_handler);
    conn->set_close_handler([this](const connection_ptr& conn){
            this->remove_connection(conn);
            });
    connection_map.emplace(conn->id(), conn);
    conn->get_loop()->run_in_loop([conn]{ conn->establish(); });
}

void server::remove_connection(const connection_ptr& conn)
{
    if (close_handler) close_handler(conn);
    conn->set_state(connection::state::closed);
    conn->get_loop()->remove_channel(conn->channel);
    // We have to remove a connection in the io loop thread
    // to prevent multiple threads from concurrently modifying the connection_map.
    loop->run_in_loop([this, id = conn->id()]{
            this->connection_map.erase(id);
            });
}

connection_ptr server::get_connection(size_t id)
{
    auto it = connection_map.find(id);
    return (it != connection_map.cend()) ? it->second : nullptr;
}

size_t server::get_connection_nums() const
{
    return connection_map.size();
}

void server::for_one(size_t id, const for_each_functor_t functor)
{
    if (!functor) return;
    loop->run_in_loop([this, id = id, functor = std::move(functor)]{
            auto it = connection_map.find(id);
            if (it == connection_map.cend()) return;
            functor(it->second);
            });
}

void server::for_each(const for_each_functor_t functor)
{
    if (!functor) return;
    loop->run_in_loop([this, functor = std::move(functor)]{
            for (auto& it : this->connection_map)
                functor(it.second);
            });
}

void server::start_io_threads(size_t thread_nums)
{
    if (thread_nums > 0) {
        io_loop_group.reset(new evloop_group(thread_nums));
    } else {
        io_loop_group.reset(new evloop_group());
    }
}

void server::start_task_threads(size_t thread_nums, enum thread_pool::policy policy)
{
    if (thread_nums > 0) {
        task_thread_pool.reset(new thread_pool(policy, thread_nums));
    } else {
        task_thread_pool.reset(new thread_pool(policy));
    }
}

void server::executor(const task_callback_t task)
{
    if (task_thread_pool) {
        task_thread_pool->executor(task);
    }
}

void server::clean_up()
{
    log_info("Server ready to exit...");
    __logger.quit();
    fprintf(stderr, "\n");
    quit();
}

void server::set_nodelay(bool on)
{
    listener->nodelay = on;
}

void server::set_keepalive(bool on)
{
    listener->keepalive = on;
}

void server::set_keepalive_idle(int idle)
{
    listener->keepalive_idle = idle;
}

void server::set_keepalive_intvl(int intvl)
{
    listener->keepalive_intvl = intvl;
}

void server::set_keepalive_probes(int probes)
{
    listener->keepalive_probes = probes;
}

void server::handle_signals()
{
    // The SIGPIPE signal must be ignored, otherwise sending a message
    // to a closed connection will cause the server to exit unexpectedly.
    ignore_signal(SIGPIPE);
    add_signal(SIGINT, [this]{ this->clean_up(); });
    add_signal(SIGTERM, [this]{ this->clean_up(); });
}

void server::start()
{
    handle_signals();
    log_info("Server (%s) is running", listener->addr().to_host());
    listener->new_connection_handler = [this](int fd){ this->new_connection(fd); };
    listener->listen();
}

void server::quit()
{
    loop->quit();
}

void server::daemon()
{
    __logger.quit();
    util::daemon();
    __logger.restart();
}

}
