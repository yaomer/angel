#ifndef __ANGEL_DNS_RESOLVER_H
#define __ANGEL_DNS_RESOLVER_H

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <future>
#include <memory>
#include <variant>

#include <angel/evloop_thread.h>
#include <angel/client.h>

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

struct a_rdata;
struct ns_rdata;
struct cname_rdata;
struct mx_rdata;
struct txt_rdata;
struct soa_rdata;
struct ptr_rdata;

// RR(Resource record) Common
// for (Answer, Authority and Additional)
struct rr_base {
    rr_base() = default;
    rr_base(const rr_base&) = default;
    // When used, it must be guaranteed to point to one of its derived classes
    const a_rdata *as_a() const;
    const ns_rdata *as_ns() const;
    const cname_rdata *as_cname() const;
    const mx_rdata *as_mx() const;
    const txt_rdata *as_txt() const;
    const soa_rdata *as_soa() const;
    const ptr_rdata *as_ptr() const;
    const char *as_err() const;
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

class resolver;

struct query_context : public std::enable_shared_from_this<query_context> {
    uint16_t id;
    std::string name;
    uint16_t q_type;
    uint16_t q_class;
    std::string buf;
    std::promise<result> recv_promise;
    size_t retransmit_timer_id;
    void pack();
    void set_retransmit_timer(resolver *);
    void send_query(resolver *r);
};

class resolver {
public:
    resolver();
    resolver(const resolver&) = delete;
    resolver& operator=(const resolver&) = delete;
    // auto f = query()
    // if f.valid() == false, argument error
    // if f.get().front()->type == ERROR, resolver error
    result_future query(std::string_view name, int type);
    // show result info
    static void show(const result_future&);
private:
    void unpack(angel::buffer& res_buf);
    result_future query(std::string_view name, uint16_t q_type, uint16_t q_class);

    // The background thread runs an `evloop` to receive the response
    angel::evloop_thread receiver;
    std::unique_ptr<angel::client> cli;
    std::atomic_size_t id = 1;
    typedef std::unordered_map<uint16_t, std::shared_ptr<query_context>> QueryMap;
    typedef std::queue<std::packaged_task<void()>> DelayTaskQueue;
    QueryMap query_map;
    std::mutex query_map_mutex;
    DelayTaskQueue delay_task_queue;
    std::mutex delay_task_queue_mutex;

    friend struct query_context;
};

}
}

#endif // __ANGEL_DNS_RESOLVER_H
