#ifndef __ANGEL_DNS_RESOLVER_H
#define __ANGEL_DNS_RESOLVER_H

#include <vector>
#include <unordered_map>
#include <future>

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
    ERROR   = 128, // query error
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

struct query_context;

class cache;

class resolver {
public:
    resolver(const resolver&) = delete;
    resolver& operator=(const resolver&) = delete;
    // auto f = query()
    // if f.valid() == false, argument error
    // if f.get().front()->type == ERROR, resolver error
    // (Do not use cache)
    result_future query(std::string_view dname, int type);
    // wait_for_ms = 0: block and wait until the result is returned
    // wait_for_ms > 0: return after `wait_for_ms` (ms)
    // get A record result
    std::vector<std::string> get_addr_list(std::string_view dname, int wait_for_ms = 0);
    // get MX record result, sort by preference
    std::vector<std::string> get_mx_name_list(std::string_view dname, int wait_for_ms = 0);
    // show result info
    static void show(const result_future&);
    ////////////////////////////////
    // Singleton
    ////////////////////////////////
    // Cache is only valid for get_addr_list()
    static resolver *get_resolver()
    {
        static resolver ins;
        return &ins;
    }
private:
    resolver();
    ~resolver();

    void send_query(query_context *qc);
    void set_retransmit_timer(query_context *qc);
    void retransmit(uint16_t id);
    void notify(query_context *qc, result res);
    void err_notify(query_context *qc, const char *err);

    void unpack(angel::buffer& res_buf);
    result_future query(std::string_view name, uint16_t q_type, uint16_t q_class);
    void do_query(query_context *qc);

    // The background thread runs an `evloop` to receive the response
    angel::evloop_thread receiver;
    angel::evloop *loop; // receiver.get_loop()
    std::unique_ptr<angel::client> cli;
    std::unordered_map<uint16_t, std::unique_ptr<query_context>> query_map;
    // Only Cache A record
    std::unique_ptr<cache> cache;
};

}
}

#endif // __ANGEL_DNS_RESOLVER_H
