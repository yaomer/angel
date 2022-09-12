#include <angel/sockops.h>
#include <angel/util.h>

#include <netinet/in.h>

#include "resolver.h"

namespace angel {

namespace dns {

// RR TYPE
#define TYPE_A          1 // a host address
#define TYPE_NS         2 // an authoritative name server
#define TYPE_CNAME      5 // the canonical name for an alias
#define TYPE_SOA        6 // marks the start of a zone of authority
#define TYPE_WKS       11 // a well known service description
#define TYPE_PTR       12 // a domain name pointer
#define TYPE_HINFO     13 // host information
#define TYPE_MINFO     14 // mailbox or mail list information
#define TYPE_MX        15 // mail exchange
#define TYPE_TXT       16 // text strings

static std::unordered_map<std::string_view, uint16_t> type_map = {
    { "A",      1 },
    { "NS",     2 },
    { "CNAME",  5 },
    { "SOA",    6 },
    { "WKS",   11 },
    { "PTR",   12 },
    { "HINFO", 13 },
    { "MINFO", 14 },
    { "MX",    15 },
    { "TXT",   16 },
};

// RR CLASS
#define CLASS_IN    1 // the internet

#define DNS_SERVER_IP   "192.168.43.1"
#define DNS_SERVER_PORT 53

template <typename Ptr>
static inline const char *charptr(Ptr p)
{
    return reinterpret_cast<const char*>(p);
}

static inline uint16_t u16(const char*& p)
{
    uint16_t u = *reinterpret_cast<uint16_t*>(const_cast<char*>(p));
    p += sizeof(u);
    return u;
}

static inline uint32_t u32(const char*& p)
{
    uint32_t u = *reinterpret_cast<uint32_t*>(const_cast<char*>(p));
    p += sizeof(u);
    return u;
}

// size limits
static const uint16_t max_label_size    = 63; // 00xx xxxx
static const uint16_t max_name_size     = 255;
static const uint16_t max_udp_size      = 512;

// <www.example.com> => <3www7example3com0>
static std::string to_dns_name(std::string_view name)
{
    std::string res;
    auto labels = angel::util::split(name.data(), name.data() + name.size(), '.');
    for (auto& label : labels) {
        if (label.size() > max_label_size) return "";
        res.push_back(label.size());
        res.append(label);
    }
    res.push_back(0);
    return res.size() > max_name_size ? "" : res;
}

// offset:      11xx xxxx xxxx xxxx
// label char:  00xx xxxx
static std::string parse_dns_name(const char *start, const char*& cur)
{
    std::string res;
    while (true) {
        if ((*cur & 0xc0) == 0xc0) {
            uint16_t offset = ntohs(u16(cur)) & 0x3fff;
            const char *p = start + offset;
            return res + parse_dns_name(start, p);
        }
        uint8_t count = *cur++;
        if (count == 0) break;
        res.append(std::string(cur, count));
        res.push_back('.');
        cur += count;
    }
    if (!res.empty()) res.pop_back();
    return res;
}

static std::string to_ip(const char*& p)
{
    struct in_addr addr;
    addr.s_addr = u32(p);
    return angel::sockops::to_host_ip(&addr);
}

struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t query;
    uint16_t answer;
    uint16_t authority;
    uint16_t additional;
};

void query_context::pack(std::string_view name, uint16_t q_type, uint16_t q_class)
{
    dns_header hdr;
    hdr.id          = htons(id);
    hdr.flags       = 0x0100;
    hdr.query       = htons(1);
    hdr.answer      = 0;
    hdr.authority   = 0;
    hdr.additional  = 0;

    q_type  = htons(q_type);
    q_class = htons(q_class);

    buf.resize(sizeof(hdr));
    memcpy(&buf[0], &hdr, sizeof(hdr));
    buf.append(name);
    buf.append(charptr(&q_type), sizeof(q_type));
    buf.append(charptr(&q_class), sizeof(q_class));
}

enum {
    NoError,
    FormatError,
    ServerFailure,
    NameError,
    NotImplemented,
    Refused,
    Unmatch = 16,
};

resolver::resolver()
{
    angel::client_options ops;
    ops.protocol = "udp";
    ops.is_reconnect = true;
    ops.is_quit_loop = false;
    loop = loop_thread.wait_loop();
    cli.reset(new angel::client(loop, angel::inet_addr(DNS_SERVER_IP, DNS_SERVER_PORT), ops));
    cli->set_connection_handler([this](const angel::connection_ptr& conn){
            lock_t lk(delay_task_queue_mutex);
            while (!delay_task_queue.empty()) {
                delay_task_queue.front()();
                delay_task_queue.pop();
            }
            });
    cli->set_message_handler([this](const angel::connection_ptr& conn, angel::buffer& buf){
            int rcode = this->unpack(buf);
            (void)(rcode);
            });
    cli->start();
}

int resolver::unpack(angel::buffer& res_buf)
{
    const char *p = res_buf.peek();

    uint16_t id = ntohs(u16(p));
    bool qr = p[0] >> 7;
    bool ra = p[1] >> 7;
    uint8_t rcode = p[1] & 0x0f;
    p += 2;

    if (rcode != NoError) return rcode;
    assert(qr);
    assert(ra);

    QueryMap::iterator it;
    {
        std::lock_guard<std::mutex> mlock(query_map_mutex);
        it = query_map.find(id);
        if (it == query_map.end()) return Unmatch;
    }

    uint16_t query      = ntohs(u16(p));
    uint16_t answer     = ntohs(u16(p));
    uint16_t authority  = ntohs(u16(p));
    uint16_t additional = ntohs(u16(p));

    auto name           = parse_dns_name(res_buf.peek(), p);
    uint16_t q_type     = ntohs(u16(p));
    uint16_t q_class    = ntohs(u16(p));

    result res;
    switch (q_type) {
    case TYPE_A: res = a_res_t(); break;
    case TYPE_MX: res = mx_res_t(); break;
    }

    for (int i = 0; i < answer; i++) {
        auto rr_name        = parse_dns_name(res_buf.peek(), p);
        uint16_t rr_type    = ntohs(u16(p));
        uint16_t rr_class   = ntohs(u16(p));
        uint32_t rr_ttl     = ntohl(u32(p));
        uint16_t rd_len     = ntohs(u16(p));
        switch (q_type) {
        case TYPE_A:
            {
                std::get<a_res_t>(res).emplace_back(to_ip(p));
            }
            break;
        case TYPE_MX:
            {
                uint16_t preference = ntohs(u16(p)); // lower values are preferred
                auto exchange_name = parse_dns_name(res_buf.peek(), p);
                std::get<mx_res_t>(res).emplace_back(preference, std::move(exchange_name));
            }
            break;
        }
    }

    it->second->recv_promise.set_value(std::move(res));
    {
        std::lock_guard<std::mutex> mlock(query_map_mutex);
        query_map.erase(it);
    }

    res_buf.retrieve(p - res_buf.peek());
    return NoError;
}

void resolver::delay_send(resolver *r, query_context *qc)
{
    r->cli->conn()->send(qc->buf);
}

result_future resolver::query(std::string_view name, uint16_t q_type, uint16_t q_class)
{
    auto *qc = new query_context();
    qc->id = id++ % 65536;
    qc->pack(name, q_type, q_class);
    auto f = qc->recv_promise.get_future();
    {
        std::lock_guard<std::mutex> mlock(query_map_mutex);
        query_map.emplace(qc->id, qc);
    }
    if (!cli->is_connected()) {
        std::packaged_task<void()> t(std::bind(&delay_send, this, qc));
        {
            lock_t lk(delay_task_queue_mutex);
            delay_task_queue.emplace(std::move(t));
        }
    } else {
        cli->conn()->send(qc->buf);
    }
    return f;
}

result_future resolver::query(std::string_view name, std::string_view type)
{
    if (name == "") return result_future();
    auto dns_name = to_dns_name(name);
    if (dns_name == "") return result_future();
    if (!type_map.count(type)) return result_future();
    return query(dns_name, type_map[type], CLASS_IN);
}

}
}
