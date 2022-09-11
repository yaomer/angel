#include <angel/sockops.h>
#include <angel/util.h>

#include <netinet/in.h>

#include "resolver.h"

// RR TYPE
#define TYPE_A      1 // a host address
#define TYPE_CNAME  5 // the canonical name for an alias
#define TYPE_MX    15 // mail exchange

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

// <www.example.com> => <3www7example3com0>
static std::string name_to_dns(std::string_view name)
{
    std::string res;
    auto labels = angel::util::split(name.data(), name.data() + name.size(), '.');
    for (auto& label : labels) {
        res.push_back(label.size());
        res.append(label);
    }
    res.push_back(0);
    return res;
}

static std::string name_from_dns(const char*& p)
{
    std::string res;
    while (true) {
        uint8_t count = *p++;
        if (count == 0) break;
        res.append(std::string(p, count));
        res.push_back('.');
        p += count;
    }
    res.pop_back();
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

void query_context::pack(std::string name, uint16_t q_type, uint16_t q_class)
{
    dns_header hdr;
    hdr.id          = htons(id);
    hdr.flags       = 0x0100;
    hdr.query       = htons(1);
    hdr.answer      = 0;
    hdr.authority   = 0;
    hdr.additional  = 0;

    name    = name_to_dns(name);
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
    cli.reset(new angel::client(loop_thread.wait_loop(), angel::inet_addr(DNS_SERVER_IP, DNS_SERVER_PORT), ops));
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

    auto name           = name_from_dns(p);
    uint16_t q_type     = ntohs(u16(p));
    uint16_t q_class    = ntohs(u16(p));

    result res;
    assert(answer > 0);
    for (int i = 0; i < answer; i++) {
        uint16_t rr_name    = ntohs(u16(p));
        uint16_t rr_type    = ntohs(u16(p));
        uint16_t rr_class   = ntohs(u16(p));
        uint32_t rr_ttl     = ntohl(u32(p));
        uint16_t rd_len     = ntohs(u16(p));
        switch (q_type) {
        case TYPE_A:
            res.emplace_back(to_ip(p));
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

std::shared_future<result> resolver::query(const std::string& name, uint16_t q_type, uint16_t q_class)
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

std::shared_future<result> resolver::query(const std::string& name, const std::string& type)
{
    static std::unordered_map<std::string_view, uint16_t> typemap = {
        { "A",      1 },
        { "CNAME",  5 },
        { "MX",    15 },
    };
    assert(typemap.count(type));
    return query(name, typemap[type], CLASS_IN);

}
