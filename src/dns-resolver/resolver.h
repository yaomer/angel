#ifndef __ANGEL_RESOLVER_H
#define __ANGEL_RESOLVER_H

#include <angel/evloop_thread.h>
#include <angel/client.h>

#include <vector>
#include <queue>
#include <unordered_map>
#include <future>
#include <memory>
#include <variant>

namespace angel {

namespace dns {

enum type {
    A       =  1, // a host address
    NS      =  2, // an authoritative name server
    CNAME   =  5, // the canonical name for an alias
    SOA     =  6, // marks the start of a zone of authority
    // WKS     = 11, // a well known service description
    PTR     = 12, // a domain name pointer
    // HINFO   = 13, // host information
    // MINFO   = 14, // mailbox or mail list information
    MX      = 15, // mail exchange
    TXT     = 16, // text strings
    //============================
    ERROR   = 17, // query error
};

struct rr_base {
    rr_base() = default;
    rr_base(const rr_base&) = default;
    std::string name;
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t len;
};

struct a_rdata : rr_base {
    a_rdata(const rr_base& rr) : rr_base(rr) {  }
    std::string addr;
};

struct ns_rdata : rr_base {
    ns_rdata(const rr_base& rr) : rr_base(rr) {  }
    std::string ns_name;
};

// A domain name can usually only point to one authoritative domain name
struct cname_rdata : rr_base {
    cname_rdata(const rr_base& rr) : rr_base(rr) {  }
    std::string cname;
};

struct mx_rdata : rr_base {
    mx_rdata(const rr_base& rr) : rr_base(rr) {  }
    uint16_t preference; // lower values are preferred
    std::string exchange_name;
};

struct txt_rdata : rr_base {
    txt_rdata(const rr_base& rr) : rr_base(rr) {  }
    std::string str;
};

// most used by name server
struct soa_rdata : rr_base {
    soa_rdata(const rr_base& rr) : rr_base(rr) {  }
    std::string mname;
    std::string rname;
    uint32_t    serial;  // version number
    uint32_t    refresh; // time interval(s)
    uint32_t    retry;   // time interval(s)
    uint32_t    expire;  // time value(s)
    uint32_t    minimum; // minimum ttl(s)
};

struct ptr_rdata : rr_base {
    ptr_rdata(const rr_base& rr) : rr_base(rr) {  }
    std::string ptr_name;
};

typedef std::unique_ptr<rr_base> rr_base_ptr;
typedef std::vector<rr_base_ptr> result;
typedef std::shared_future<result> result_future;

static inline const a_rdata *get_a(const rr_base_ptr& rr)
{
    return static_cast<const a_rdata*>(rr.get());
}

static inline const ns_rdata *get_ns(const rr_base_ptr& rr)
{
    return static_cast<const ns_rdata*>(rr.get());
}

static inline const cname_rdata *get_cname(const rr_base_ptr& rr)
{
    return static_cast<const cname_rdata*>(rr.get());
}

static inline const mx_rdata *get_mx(const rr_base_ptr& rr)
{
    return static_cast<const mx_rdata*>(rr.get());
}

static inline const txt_rdata *get_txt(const rr_base_ptr& rr)
{
    return static_cast<const txt_rdata*>(rr.get());
}

static inline const soa_rdata *get_soa(const rr_base_ptr& rr)
{
    return static_cast<const soa_rdata*>(rr.get());
}

static inline const ptr_rdata *get_ptr(const rr_base_ptr& rr)
{
    return static_cast<const ptr_rdata*>(rr.get());
}

static inline const char *get_err(const rr_base_ptr& rr)
{
    return rr->name.c_str();
}

struct query_context {
    uint16_t id;
    std::string name;
    uint16_t q_type;
    uint16_t q_class;
    std::string buf;
    std::promise<result> recv_promise;
    void pack();
};

typedef std::lock_guard<std::mutex> lock_t;

class resolver {
public:
    resolver();
    // auto f = query()
    // if f.valid() == false, argument error
    // if f.get().front()->type == ERROR, resolver error
    result_future query(std::string_view name, int type);
private:
    static void delay_send(resolver *r, query_context *qc);
    void unpack(angel::buffer& res_buf);
    result_future query(std::string_view name, uint16_t q_type, uint16_t q_class);

    // The background thread runs an `evloop` to receive the response
    angel::evloop_thread loop_thread;
    angel::evloop *loop; // -> loop_thread.get_loop()
    std::unique_ptr<angel::client> cli;
    std::atomic_size_t id = 1;
    typedef std::unordered_map<uint16_t, std::unique_ptr<query_context>> QueryMap;
    typedef std::queue<std::packaged_task<void()>> DelayTaskQueue;
    QueryMap query_map;
    std::mutex query_map_mutex;
    DelayTaskQueue delay_task_queue;
    std::mutex delay_task_queue_mutex;
};

}
}

#endif // __ANGEL_RESOLVER_H
