#ifndef __ANGEL_SMTPLIB_H
#define __ANGEL_SMTPLIB_H

#include <angel/evloop_thread.h>
#include <angel/resolver.h>

#include <vector>
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>

namespace angel {
namespace smtplib {

struct email {
    std::string from;
    std::vector<std::string> to;
    // From, To, Subject
    std::unordered_map<std::string, std::string> headers;
    std::string data;
};

struct send_task;

struct result {
    bool is_ok;
    std::string err;
};

typedef std::shared_future<result> result_future;

class sender {
public:
    sender();
    ~sender();
    sender(const sender&) = delete;
    sender& operator=(const sender&) = delete;

    result_future send(std::string_view host, int port,
                       std::string_view username, std::string_view password,
                       const email& mail);
private:
    dns::resolver *resolver;
    evloop_thread send_thread;
    std::unordered_map<size_t, std::shared_ptr<send_task>> task_map;
    std::mutex task_map_mutex;
    std::atomic_size_t id;

    friend struct send_task;
};

}
}

#endif // __ANGEL_SMTPLIB_H
