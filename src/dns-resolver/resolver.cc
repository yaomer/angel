#include <angel/sockops.h>
#include <angel/util.h>
#include <angel/logger.h>

#include <netinet/in.h>

#include <fstream>

#include "resolver.h"

namespace angel {

namespace dns {

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

//  Header
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      ID                       |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |QR|   Opcode  |AA|TC|RD|RA| Z|AD|CD|   RCODE   |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    QDCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ANCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    NSCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ARCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  QR       0   query
//           1   response
//  OPCODE   0   standard query
//  (4)      1   inverse query
//           2   server status request
//           3-15 reserved
//  AA       authoritative answer
//  TC       truncate
//  RD       recursion desired
//  RA       recursion available
//  Z,AD,CD  0
//  RCODE    0   no error
//  (4)      1   format error
//           2   server failure
//           3   name error
//           4   not implemented
//           5   refused
//           6-15 reserved

void query_context::pack()
{
    struct dns_header {
        uint16_t id;
        uint16_t flags;
        uint16_t query;
        uint16_t answer;
        uint16_t authority;
        uint16_t additional;
    } hdr;

    hdr.id          = htons(id);
    hdr.flags       = 0x0100;
    hdr.query       = htons(1);
    hdr.answer      = 0;
    hdr.authority   = 0;
    hdr.additional  = 0;

    buf.resize(sizeof(hdr));
    memcpy(&buf[0], &hdr, sizeof(hdr));

    q_type  = htons(q_type);
    q_class = htons(q_class);

    buf.append(name);
    buf.append(charptr(&q_type), sizeof(q_type));
    buf.append(charptr(&q_class), sizeof(q_class));
}

static const char *resolv_conf = "/etc/resolv.conf";
static const int name_server_port = 53;

// Pick a name server for an ipv4 address
static std::string parse_resolv_conf()
{
    char buf[BUFSIZ];
    std::ifstream ifs(resolv_conf);
    if (!ifs.is_open()) {
        log_fatal("can't open %s", resolv_conf);
    }
    while (ifs.getline(buf, sizeof(buf))) {
        const char *p = buf;
        const char *end = buf + strlen(buf);
        while (p < end && isspace(*p)) p++;
        if (p == end || *p == '#') continue;
        if (strncmp(p, "nameserver", 10) == 0) {
            p += 10;
            while (p < end && isspace(*p)) p++;
            std::string addr;
            while (p < end && !isspace(*p))
                addr.push_back(*p++);
            if (addr.find(".") != std::string::npos)
                return addr;
        }
    }
    return "";
}

resolver::resolver()
{
    auto name_server_addr = parse_resolv_conf();
    if (name_server_addr == "") {
        log_fatal("can't find a name server address");
    }

    angel::client_options ops;
    ops.protocol = "udp";
    ops.is_reconnect = true;
    ops.is_quit_loop = false;
    loop = loop_thread.wait_loop();
    cli.reset(new angel::client(loop, angel::inet_addr(name_server_addr, name_server_port), ops));
    cli->set_connection_handler([this](const angel::connection_ptr& conn){
            lock_t lk(delay_task_queue_mutex);
            while (!delay_task_queue.empty()) {
                delay_task_queue.front()();
                delay_task_queue.pop();
            }
            });
    cli->set_message_handler([this](const angel::connection_ptr& conn, angel::buffer& buf){
            this->unpack(buf);
            buf.retrieve_all();
            });
    cli->start();
}

enum {
    NoError,
    FormatError,
    ServerFailure,
    NameError,
    NotImplemented,
    Refused,
};

static const std::unordered_map<int, std::string_view> rcode_map = {
    { FormatError,      "FormatError" },
    { ServerFailure,    "ServerFailure" },
    { NameError,        "NameError" },
    { NotImplemented,   "NotImplemented" },
    { Refused,          "Refused" },
};

static void parse_answer_rrs(result& res, const char*& p, const char *start, int answer)
{
    rr_base rr;
    for (int i = 0; i < answer; i++) {
        rr.name     = parse_dns_name(start, p);
        rr.type     = ntohs(u16(p));
        rr._class   = ntohs(u16(p));
        rr.ttl      = ntohl(u32(p));
        rr.len      = ntohs(u16(p));
        switch (rr.type) {
        case A:
            {
                a_rdata *a = new a_rdata(rr);
                a->addr = std::move(to_ip(p));
                res.emplace_back(dynamic_cast<rr_base*>(a));
            }
            break;
        case NS:
            {
                ns_rdata *ns = new ns_rdata(rr);
                ns->ns_name = parse_dns_name(start, p);
                res.emplace_back(dynamic_cast<rr_base*>(ns));
            }
            break;
        case CNAME:
            {
                cname_rdata *cname = new cname_rdata(rr);
                cname->cname = parse_dns_name(start, p);
                res.emplace_back(dynamic_cast<rr_base*>(cname));
            }
            break;
        case MX:
            {
                mx_rdata *mx = new mx_rdata(rr);
                mx->preference = ntohs(u16(p));
                mx->exchange_name = parse_dns_name(start, p);
                res.emplace_back(dynamic_cast<rr_base*>(mx));
            }
            break;
        case TXT:
            {
                txt_rdata *txt = new txt_rdata(rr);
                uint8_t txt_len = *p++;
                txt->str.assign(p, p + txt_len);
                p += txt_len;
                res.emplace_back(dynamic_cast<rr_base*>(txt));
            }
            break;
        case SOA:
            {
                soa_rdata *soa = new soa_rdata(rr);
                soa->mname     = parse_dns_name(start, p);
                soa->rname     = parse_dns_name(start, p);
                soa->serial    = ntohl(u32(p));
                soa->refresh   = ntohl(u32(p));
                soa->retry     = ntohl(u32(p));
                soa->expire    = ntohl(u32(p));
                soa->minimum   = ntohl(u32(p));
                res.emplace_back(dynamic_cast<rr_base*>(soa));
            }
            break;
        case PTR:
            {
                ptr_rdata *ptr = new ptr_rdata(rr);
                ptr->ptr_name = parse_dns_name(start, p);
                res.emplace_back(dynamic_cast<rr_base*>(ptr));
            }
            break;
        }
    }
}

static bool is_same_name(std::string_view name, const char*& p)
{
    for (auto c : name) {
        if (c != *p++) return false;
    }
    return true;
}

void resolver::unpack(angel::buffer& res_buf)
{
    const char *p = res_buf.peek();

    uint16_t id = ntohs(u16(p));
    bool qr = p[0] >> 7;
    bool ra = p[1] >> 7;
    uint8_t rcode = p[1] & 0x0f;
    p += 2;

    if (!qr) return;

    QueryMap::iterator it;
    {
        std::lock_guard<std::mutex> mlock(query_map_mutex);
        it = query_map.find(id);
        if (it == query_map.end()) return;
    }

    result res;
    if (rcode != NoError) {
        rr_base *rr = new rr_base();
        rr->type = ERROR;
        rr->name = rcode_map.at(rcode);
        res.emplace_back(rr);
        it->second->recv_promise.set_value(std::move(res));
        {
            std::lock_guard<std::mutex> mlock(query_map_mutex);
            query_map.erase(it);
        }
        return;
    }

    uint16_t query      = ntohs(u16(p));
    uint16_t answer     = ntohs(u16(p));
    uint16_t authority  = ntohs(u16(p));
    uint16_t additional = ntohs(u16(p));

    (void)(query);
    (void)(authority);
    (void)(additional);

    if (!is_same_name(it->second->name, p)) return;
    if (it->second->q_type != u16(p)) return;
    if (it->second->q_class != u16(p)) return;

    parse_answer_rrs(res, p, res_buf.peek(), answer);

    it->second->recv_promise.set_value(std::move(res));
    {
        std::lock_guard<std::mutex> mlock(query_map_mutex);
        query_map.erase(it);
    }
}

void query_context::set_retransmit_timer(resolver *r)
{
    // TODO: 指数退避
    retransmit_timer_id = r->loop->run_after(3000, [r, qc = shared_from_this()](){
            lock_t lk(r->query_map_mutex);
            if (r->query_map.count(qc->id)) {
                r->cli->conn()->send(qc->buf);
                qc->set_retransmit_timer(r);
            } else {
                r->loop->cancel_timer(qc->retransmit_timer_id);
            }
            });
}

void resolver::delay_send(resolver *r, query_context *qc)
{
    r->cli->conn()->send(qc->buf);
    qc->set_retransmit_timer(r);
}

result_future resolver::query(std::string_view name, uint16_t q_type, uint16_t q_class)
{
    auto *qc = new query_context();
    qc->id = id++ % 65536;
    qc->name = name;
    qc->q_type = q_type;
    qc->q_class = q_class;
    qc->pack();
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
        qc->set_retransmit_timer(this);
    }
    return f;
}

#define CLASS_IN 1 // the internet

result_future resolver::query(std::string_view name, int type)
{
    if (name == "") return result_future();
    auto dns_name = to_dns_name(name);
    if (dns_name == "") return result_future();
    // check type
    return query(dns_name, type, CLASS_IN);
}

}
}