//
// Async DNS Resolver
// See https://www.rfc-editor.org/rfc/rfc1035.html
//

#include <angel/resolver.h>

#include <iostream>
#include <fstream>
#include <chrono>

#include <angel/sockops.h>
#include <angel/util.h>
#include <angel/logger.h>
#include <angel/backoff.h>

#include "cache.h"

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
    auto labels = util::split(name, '.');
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
    return sockops::to_host_ip(&addr);
}

struct query_context : public std::enable_shared_from_this<query_context> {
    uint16_t id;
    std::string name;
    uint16_t q_type;
    uint16_t q_class;
    std::string buf;
    std::promise<result> recv_promise;
    size_t retransmit_timer_id;
    ExponentialBackoff backoff;
    resolver *resolver;

    void pack();
    void set_retransmit_timer();
    void send_query();
    void notify(result&& res);
};

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
    Assert(buf.size() <= max_udp_size);
}

static const char *resolv_conf = "/etc/resolv.conf";
static const int name_server_port = 53;

// Pick a name server for an ipv4 address
static std::string parse_resolv_conf()
{
    auto res = util::parse_conf(resolv_conf);
    for (auto& arg : res) {
        if (arg[0] == "nameserver") {
            if (util::check_ip(arg[1])) {
                return arg[1];
            }
        }
    }
    return "";
}

typedef std::lock_guard<std::mutex> lock_t;

resolver::resolver()
{
    auto name_server_addr = parse_resolv_conf();
    if (name_server_addr == "") {
        log_fatal("(resolver) can't find a name server address");
    }
    log_info("(resolver) found a name server (%s)", name_server_addr.c_str());

    client_options ops;
    ops.protocol = "udp";
    ops.is_reconnect = true;
    cli.reset(new client(receiver.get_loop(), inet_addr(name_server_addr, name_server_port), ops));
    cli->set_connection_handler([this](const connection_ptr& conn){
            lock_t lk(delay_task_queue_mutex);
            while (!delay_task_queue.empty()) {
                delay_task_queue.front()();
                delay_task_queue.pop();
            }
            });
    cli->set_message_handler([this](const connection_ptr& conn, buffer& buf){
            this->unpack(buf);
            buf.retrieve_all();
            });

    cache.reset(new class cache());
    receiver.get_loop()->run_every(1000, [this]{ this->cache->evict(); });

    cli->start();
}

resolver::~resolver()
{
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
                a->addr = to_ip(p);
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

void resolver::unpack(buffer& res_buf)
{
    const char *p = res_buf.peek();

    uint16_t id = ntohs(u16(p));
    bool qr = p[0] >> 7;
    bool ra = p[1] >> 7;
    uint8_t rcode = p[1] & 0x0f;
    p += 2;

    if (!qr) return;
    (void)(ra);

    QueryMap::iterator it;
    {
        lock_t lk(query_map_mutex);
        it = query_map.find(id);
        if (it == query_map.end()) return;
    }
    log_debug("(resolver) recv query(id=%zu)", it->second->id);

    result res;
    if (rcode != NoError) {
        rr_base *rr = new rr_base();
        rr->type = ERROR;
        rr->name = rcode_map.at(rcode);
        res.emplace_back(rr);
        it->second->notify(std::move(res));
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

    it->second->notify(std::move(res));
}

void query_context::notify(result&& res)
{
    if (retransmit_timer_id > 0) {
        resolver->receiver.get_loop()->cancel_timer(retransmit_timer_id);
    }
    recv_promise.set_value(std::move(res));
    {
        lock_t lk(resolver->query_map_mutex);
        resolver->query_map.erase(id);
    }
}

void query_context::set_retransmit_timer()
{
    int after = backoff.next_back_off();
    if (after == 0) {
        result res;
        rr_base *rr = new rr_base();
        rr->type = TIMEOUT;
        res.emplace_back(rr);
        notify(std::move(res));
        return;
    }
    retransmit_timer_id = resolver->receiver.get_loop()->run_after(after, [qc = shared_from_this()](){
            auto r = qc->resolver;
            bool need_retransmit;
            {
                lock_t lk(r->query_map_mutex);
                need_retransmit = r->query_map.count(qc->id);
            }
            if (need_retransmit) {
                r->cli->conn()->send(qc->buf);
                log_debug("(resolver) retransmit query(id=%zu)", qc->id);
                qc->set_retransmit_timer();
            }
            });
}

void query_context::send_query()
{
    resolver->cli->conn()->send(buf);
    log_debug("(resolver) send query(id=%zu)", id);
    set_retransmit_timer();
}

result_future resolver::query(std::string_view name, uint16_t q_type, uint16_t q_class)
{
    auto *qc = new query_context();
    qc->id = id++ % 65536;
    qc->name = name;
    qc->q_type = q_type;
    qc->q_class = q_class;
    // 1 + 2 + 4 + 8 + 16 == 31(s)
    qc->backoff = ExponentialBackoff(1000, 2, 5);
    qc->resolver = this;
    qc->pack();
    auto f = qc->recv_promise.get_future();
    {
        lock_t lk(query_map_mutex);
        query_map.emplace(qc->id, qc);
    }
    if (!cli->is_connected()) {
        std::shared_ptr<query_context> sptr(qc);
        std::weak_ptr<query_context> wptr = sptr;
        std::packaged_task<void()> task([wptr]() { if (!wptr.expired()) wptr.lock()->send_query(); });
        {
            lock_t lk(delay_task_queue_mutex);
            delay_task_queue.emplace(std::move(task));
        }
    } else {
        qc->send_query();
    }
    return f;
}

#define CLASS_IN 1 // the internet

static const std::unordered_map<int, const char *> type_map = {
    { A,        "A" },
    { NS,       "NS" },
    { CNAME,    "CNAME" },
    { SOA,      "SOA" },
    { PTR,      "PTR" },
    { MX,       "MX" },
    { TXT,      "TXT" },
};

result_future resolver::query(std::string_view name, int type)
{
    if (name == "") return result_future();
    auto dns_name = to_dns_name(name);
    if (dns_name == "") return result_future();
    auto it = type_map.find(type);
    if (it == type_map.end()) return result_future();
    log_info("(resolver) query(name=%s, type=%s)", &*name.begin(), it->second);
    return query(dns_name, type, CLASS_IN);
}

const a_rdata *rr_base::as_a() const { return static_cast<const a_rdata*>(this); }
const ns_rdata *rr_base::as_ns() const { return static_cast<const ns_rdata*>(this); }
const cname_rdata *rr_base::as_cname() const { return static_cast<const cname_rdata*>(this); }
const mx_rdata *rr_base::as_mx() const { return static_cast<const mx_rdata*>(this); }
const txt_rdata *rr_base::as_txt() const { return static_cast<const txt_rdata*>(this); }
const soa_rdata *rr_base::as_soa() const { return static_cast<const soa_rdata*>(this); }
const ptr_rdata *rr_base::as_ptr() const { return static_cast<const ptr_rdata*>(this); }
const char *rr_base::as_err() const { return name.c_str(); }

static bool is_ready(result_future& f, int wait_for_ms)
{
    if (wait_for_ms <= 0) return true;
    return f.wait_for(std::chrono::milliseconds(wait_for_ms)) == std::future_status::ready;
}

std::vector<std::string> resolver::get_addr_list(std::string_view name, int wait_for_ms)
{
    std::vector<std::string> res;

    if (cache) {
        auto *p = cache->get(name);
        if (p) return p->addr_list;
    }

    auto f = query(name, A);

    if (!f.valid()) return res;

    if (!is_ready(f, wait_for_ms)) return res;

    uint32_t min_ttl = 0;
    for (auto& item : f.get()) {
        if (item->type == A) {
            res.emplace_back(std::move(item->as_a()->addr));
            if (cache) {
                min_ttl = min_ttl == 0 ? item->ttl : std::min(min_ttl, item->ttl);
            }
        }
    }

    if (cache && min_ttl > 0) {
        cache->put(name, res, min_ttl);
    }
    return res;
}

std::vector<std::string> resolver::get_mx_name_list(std::string_view name, int wait_for_ms)
{
    std::vector<std::string> res;

    typedef std::pair<uint16_t, std::string_view> mx_pair;
    std::vector<mx_pair> mx_list;

    auto f = query(name, MX);

    if (!f.valid()) return res;

    if (!is_ready(f, wait_for_ms)) return res;

    for (auto& item : f.get()) {
        if (item->type == MX) {
            auto mx = item->as_mx();
            mx_list.emplace_back(mx->preference, mx->exchange_name);
        }
    }
    // sort by preference
    if (!mx_list.empty()) {
        std::sort(mx_list.begin(), mx_list.end(), [](mx_pair& l, mx_pair& r){
                return l.first < r.first;
                });
        for (auto& [preference, name] : mx_list) {
            res.emplace_back(name);
        }
    }
    return res;
}

void resolver::show(const result_future& f)
{
    using std::cout;
    if (!f.valid()) {
        cout << "argument error\n";
        return;
    }
    for (auto& item : f.get()) {
        if (item->type == A) {
            auto *x = item->as_a();
            cout << x->name << " has address " << x->addr << "\n";
        } else if (item->type == NS) {
            auto *x = item->as_ns();
            cout << x->name << " name server " << x->ns_name << "\n";
        } else if (item->type == CNAME) {
            auto *x = item->as_cname();
            cout << x->name << " is an alias for " << x->cname << "\n";
        } else if (item->type == MX) {
            auto *x = item->as_mx();
            cout << x->name << " mail is handled by " << x->preference << " " << x->exchange_name << "\n";
        } else if (item->type == TXT) {
            auto *x = item->as_txt();
            cout << x->name << " descriptive text \"" << x->str << "\"\n";
        } else if (item->type == PTR) {
            auto *x = item->as_ptr();
            cout << x->name << " is the address of " << x->ptr_name << "\n";
        } else if (item->type == SOA) {
            auto *x = item->as_soa();
            cout << x->name << " has SOA record " << x->mname << " " << x->rname << " "
                 << x->serial << " " << x->refresh << " " << x->retry << " " << x->expire
                 << " " << x->minimum << "\n";
        } else if (item->type == ERROR) {
            cout << item->as_err() << "\n";
        }
    }
}

}
}
