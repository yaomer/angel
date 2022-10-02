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

struct send_task;

struct result {
    bool is_ok;
    std::string err;
};

typedef std::shared_future<result> result_future;

class smtp {
public:
    smtp();
    ~smtp();
    smtp(const smtp&) = delete;
    smtp& operator=(const smtp&) = delete;

    result_future send_mail(std::string_view host, int port,
                            std::string_view username, std::string_view password,
                            std::string_view sender, // sender mailbox
                            const std::vector<std::string>& receivers, // receiver mailbox list
                            std::string_view data // mail data
                            );
private:
    dns::resolver *resolver;
    evloop_thread sender;
    std::unordered_map<size_t, std::shared_ptr<send_task>> task_map;
    std::mutex task_map_mutex;
    std::atomic_size_t id;

    friend struct send_task;
};

}
}

#endif // __ANGEL_SMTPLIB_H
