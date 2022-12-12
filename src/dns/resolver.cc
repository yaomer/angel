//
// Async DNS Resolver
// See https://www.rfc-editor.org/rfc/rfc1035.html
//

#include <angel/resolver.h>

#include <iostream>
#include <chrono>
#include <random>
#include <numeric>

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
//
// Return "" If the domain name is illegal.
static std::string to_dns_name(std::string_view name)
{
    std::string res;

    // Remove the root '.' if there is.
    if (name.back() == '.') name.remove_suffix(1);
    if (name.empty()) return "";

    auto labels = util::split(name, '.');
    for (auto& label : labels) {
        if (label.size() == 0 || label.size() > max_label_size) return "";
        res.push_back((char)label.size());
        res.append(label);
    }
    res.push_back(0);
    return res.size() > max_name_size ? "" : res;
}

// The Internet uses a special domain to support gateway location
// and Internet address to host mapping.
//
// The domain begins at in-addr.arpa and has a substructure which
// follows the Internet addressing structure.
//
// Thus data for Internet address 10.2.0.52 is located at
// domain name 52.0.2.10.in-addr.arpa.
//
static std::string to_arpa_name(std::string_view name)
{
    auto r = util::split(name, '.');
    if (r.size() != 4) return "";
    return util::concat(r[3], ".", r[2], ".", r[1], ".", r[0], ".in-addr.arpa");
}

// In order to reduce the size of messages, the domain system utilizes
// a compression scheme which eliminates the repetition of domain names
// in a message. In this scheme, an entire domain name or a list of
// labels at the end of a domain name is replaced with a pointer to
// a prior occurance of the same name.

// offset:      11xx xxxx xxxx xxxx
// label char:  00xx xxxx
//
// For example, F.ISI.ARPA, FOO.F.ISI.ARPA and ARPA,
// these domain names might be represented as:
//
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 20 |           1           |           F           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 22 |           3           |           I           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 24 |           S           |           I           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 26 |           4           |           A           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 28 |           R           |           P           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 30 |           A           |           0           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 40 |           3           |           F           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 42 |           O           |           O           |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 44 | 1  1|                20                       |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 64 | 1  1|                26                       |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// 92 |           0           |                       |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Return "" if parse error.
static std::string parse_dns_name(const char *start, const char *end, const char*& cur)
{
    std::string res;
    while (cur < end) {
        if ((*cur & 0xc0) == 0xc0) { // A pointer
            if (cur + 2 > end) return ""; // At least need 2 bytes.
            uint16_t offset = ntohs(u16(cur)) & 0x3fff;
            const char *p = start + offset;
            if (p >= end) return ""; // Invalid offset
            return res + parse_dns_name(start, end, p);
        }
        uint8_t count = *cur++;
        // A sequence of labels ending in a zero octet.
        if (count == 0) {
            Assert(res.back() == '.');
            return res;
        }
        if (cur + count >= end) return "";
        res.append(cur, count).push_back('.');
        cur += count;
    }
    return "";
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
//  QDCOUNT  the number of entries in the question section.
//  ANCOUNT  the number of resource records in the answer section.
//  NSCOUNT  the number of name server resource records in the authority records section.
//  ARCOUNT  the number of resource records in the additional records section.

//  Question section format
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  /                     QNAME                     /
//  /                   (Variable)                  /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QTYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QCLASS                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
//  QNAME   a domain name represented as a sequence of labels, where
//          each label consists of a length octet followed by that
//          number of octets. The domain name terminates with the
//          zero length octet for the null label of the root. Note
//          that this field may be an odd number of octets; no
//          padding is used.
//
//  QTYPE   a two octet code which specifies the type of the query.
//          The values for this field include all codes valid for a
//          TYPE field, together with some more general codes which
//          can match more than one type of RR.
//
//  QCLASS  a two octet code that specifies the class of the query.
//          For example, the QCLASS field is IN for the Internet.

struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
};

struct query_context {
    uint16_t id; // dns transaction id
    std::string name; // query origin name
    std::string q_name;
    uint16_t q_type;
    uint16_t q_class;
    std::string buf;
    std::promise<result> res_promise;
    size_t retransmit_timer_id;
    ExponentialBackoff backoff;

    void pack()
    {
        dns_header hdr;

        hdr.id          = htons(id);
        hdr.flags       = htons(0x0100);
        hdr.qd_count    = htons(1);
        hdr.an_count    = htons(0);
        hdr.ns_count    = htons(0);
        hdr.ar_count    = htons(0);

        buf.resize(sizeof(hdr));
        memcpy(&buf[0], &hdr, sizeof(hdr));

        uint16_t n_type  = htons(q_type);
        uint16_t n_class = htons(q_class);

        buf.append(q_name);
        buf.append(charptr(&n_type), sizeof(n_type));
        buf.append(charptr(&n_class), sizeof(n_class));
        Assert(buf.size() <= max_udp_size);
    }
};

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

static const int ScanCacheInterval = 1000;

resolver::resolver()
{
    auto name_server_addr = parse_resolv_conf();
    if (name_server_addr == "") {
        log_fatal("(resolver) Could not find a name server address in %s", resolv_conf);
    }
    log_info("(resolver) found a name server (%s)", name_server_addr.c_str());

    loop = receiver.get_loop();

    client_options ops;
    ops.protocol = "udp";
    ops.is_reconnect = true;
    cli.reset(new client(loop, inet_addr(name_server_addr, name_server_port), ops));
    cli->set_connection_failure_handler([name_server_addr]{
            log_fatal("(resolver) Connect to (%s) failed", name_server_addr.c_str());
            });
    cli->set_message_handler([this](const connection_ptr& conn, buffer& buf){
            this->unpack(buf);
            buf.retrieve_all();
            });

    cache.reset(new class cache());
    loop->run_every(ScanCacheInterval, [this]{ this->cache->evict(); });

    cli->start();
}

resolver::~resolver()
{
}

void resolver::notify(query_context *qc, result res)
{
    Assert(loop->is_io_loop_thread());
    loop->cancel_timer(qc->retransmit_timer_id);
    qc->res_promise.set_value(std::move(res));
    query_map.erase(qc->id);
}

void resolver::err_notify(query_context *qc, const char *err)
{
    result res;
    auto *rr = new rr_base();
    rr->type = Error;
    rr->name = err;
    res.emplace_back(rr);
    notify(qc, std::move(res));
}

void resolver::send_query(query_context *qc)
{
    if (!cli->is_connected()) {
        // Because of async connection, it is necessary to wait for
        // the connection to be successfully established after
        // the new connection is created.
        while (!cli->is_connected()) ;
    }
    cli->conn()->send(qc->buf);
}

void resolver::set_retransmit_timer(query_context *qc)
{
    int after = qc->backoff.next_back_off();
    if (after == 0) {
        err_notify(qc, "Query timed out");
        return;
    }
    qc->retransmit_timer_id = loop->run_after(after, [this, id = qc->id]{
            retransmit(id);
            });
}

void resolver::retransmit(uint16_t id)
{
    auto it = query_map.find(id);
    if (it == query_map.end()) return;
    auto *qc = it->second.get();
    send_query(qc);
    log_info("(resolver) Retransmit query(id=%d)", id);
    set_retransmit_timer(qc);
}

enum RCODE {
    NoError,
    FormatError,
    ServerFailure,
    NameError,
    NotImplemented,
    Refused,
};

static const char *get_rcode_str(int rcode)
{
    switch (rcode) {
    case FormatError:   return "Format Error";
    case ServerFailure: return "Server Failure";
    case NameError:     return "Name Error";
    case NotImplemented:return "Not Implemented";
    case Refused:       return "Refused";
    default: return "Unknown Error";
    }
}

// Each resource record has the following format:
//                                 1  1  1  1  1  1
//   0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                      NAME                     /
// /                   (Variable)                  /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                      TYPE                     |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                     CLASS                     |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                      TTL                      |
// |                                               |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                   RDLENGTH                    |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
// /                     RDATA                     /
// /                   (Variable)                  /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// NAME     a domain name to which this resource record pertains.
//
// TYPE     two octets containing one of the RR type codes. This
//          field specifies the meaning of the data in the RDATA
//          field.
//
// CLASS    two octets which specify the class of the data in the
//          RDATA field.
//
// TTL      a 32 bit unsigned integer that specifies the time
//          interval (in seconds) that the resource record may be
//          cached before it should be discarded. Zero values are
//          interpreted to mean that the RR can only be used for the
//          transaction in progress, and should not be cached.
//
// RDLENGTH an unsigned 16 bit integer that specifies the length in
//          octets of the RDATA field.
//
// RDATA    a variable length string of octets that describes the
//          resource. The format of this information varies
//          according to the TYPE and CLASS of the resource record.
//          For example, the if the TYPE is A and the CLASS is IN,
//          the RDATA field is a 4 octet ARPA Internet address.

static bool parse_rr_base(rr_base& rr, const char *start, const char *end, const char*& p)
{
    rr.name = parse_dns_name(start, end, p);
    if (rr.name.empty()) return false;
    Assert(rr.name.back() == '.');
    rr.name.pop_back();
    if (p + 10 >= end) return false;
    rr.type     = ntohs(u16(p));
    rr._class   = ntohs(u16(p));
    rr.ttl      = ntohl(u32(p));
    rr.len      = ntohs(u16(p));
    return rr.len > 0 && p + rr.len <= end;
}

#define CHECK_DNAME(dname, rp) do { \
    if (dname.empty()) { \
        delete rp; \
        return nullptr; \
    } \
} while (0)

// A RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                    ADDRESS                    |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// ADDRESS      A 32 bit Internet address.
//
// Hosts that have multiple Internet addresses will have
// multiple A records.
//
static rr_base *parse_a_rdata(rr_base& rr, const char*& p)
{
    if (rr.len != 4) return nullptr;

    struct in_addr in_addr;
    in_addr.s_addr = u32(p);

    const char *addr = sockops::to_host_ip(&in_addr);
    if (!addr) return nullptr;

    a_rdata *a = new a_rdata(rr);
    a->addr = addr;
    return a;
}

// MX RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                  PREFERENCE                   |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                   EXCHANGE                    /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// PREFERENCE   A 16 bit integer which specifies the preference
//              given to this RR among others at the same owner.
//              Lower values are preferred.
//
// EXCHANGE     A <domain-name> which specifies a host willing to
//              act as a mail exchange for the owner name.
//
static rr_base *parse_mx_rdata(rr_base& rr, const char *start, const char *end, const char*& p)
{
    if (rr.len <= 2) return nullptr;

    mx_rdata *mx = new mx_rdata(rr);
    mx->preference    = ntohs(u16(p));
    mx->exchange_name = parse_dns_name(start, end, p);
    CHECK_DNAME(mx->exchange_name, mx);
    return mx;
}

// CNAME RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                     CNAME                     /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// CNAME    A <domain-name> which specifies the canonical
//          or primary name for the owner. The owner name
//          is an alias.
//
static rr_base *parse_cname_rdata(rr_base& rr, const char *start, const char *end, const char*& p)
{
    cname_rdata *cname = new cname_rdata(rr);
    cname->cname = parse_dns_name(start, end, p);
    CHECK_DNAME(cname->cname, cname);
    return cname;
}

// NS RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                   NSDNAME                     /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// NSDNAME  A <domain-name> which specifies a host which should be
//          authoritative for the specified class and domain.
//
static rr_base *parse_ns_rdata(rr_base& rr, const char *start, const char *end, const char*& p)
{
    ns_rdata *ns = new ns_rdata(rr);
    ns->ns_name = parse_dns_name(start, end, p);
    CHECK_DNAME(ns->ns_name, ns);
    return ns;
}

// SOA RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                     MNAME                     /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                     RNAME                     /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                    SERIAL                     |
// |                                               |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                    REFRESH                    |
// |                                               |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                     RETRY                     |
// |                                               |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                    EXPIRE                     |
// |                                               |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |                    MINIMUM                    |
// |                                               |
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// MNAME    The <domain-name> of the name server that was the
//          original or primary source of data for this zone.
//
// RNAME    A <domain-name> which specifies the mailbox of the
//          person responsible for this zone.
//
// SERIAL   The unsigned 32 bit version number of the original copy
//          of the zone. Zone transfers preserve this value. This
//          value wraps and should be compared using sequence space
//          arithmetic.
//
// REFRESH  A 32 bit time interval before the zone should be refreshed.
//
// RETRY    A 32 bit time interval that should elapse before a
//          failed refresh should be retried.
//
// EXPIRE   A 32 bit time value that specifies the upper limit on
//          the time interval that can elapse before the zone is no
//          longer authoritative.
//
// MINIMUM  The unsigned 32 bit minimum TTL field that should be
//          exported with any RR from this zone.

static rr_base *parse_soa_rdata(rr_base& rr, const char *start, const char *end, const char*& p)
{
    soa_rdata *soa = new soa_rdata(rr);

    soa->mname = parse_dns_name(start, end, p);
    CHECK_DNAME(soa->mname, soa);
    soa->rname = parse_dns_name(start, end, p);
    CHECK_DNAME(soa->rname, soa);

    if (p + 20 > end) return nullptr;

    soa->serial    = ntohl(u32(p));
    soa->refresh   = ntohl(u32(p));
    soa->retry     = ntohl(u32(p));
    soa->expire    = ntohl(u32(p));
    soa->minimum   = ntohl(u32(p));
    return soa;
}

// TXT RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                   TXT-DATA                    /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// TXT-DATA     One or more <character-string>s.
//
// TXT RRs are used to hold descriptive text.
// The semantics of the text depends on the domain where it is found.
//
static rr_base *parse_txt_rdata(rr_base& rr, const char*& p)
{
    uint8_t len = *p++;

    if (len + 1 != rr.len) return nullptr;

    txt_rdata *txt = new txt_rdata(rr);
    txt->str.assign(p, p + len);
    p += len;
    return txt;
}

// PTR RDATA format
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// /                   PTRDNAME                    /
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//
// PTRDNAME     A <domain-name> which points to some location
//              in the domain name space.
static rr_base *parse_ptr_rdata(rr_base& rr, const char *start, const char *end, const char*& p)
{
    ptr_rdata *ptr = new ptr_rdata(rr);
    ptr->ptr_name = parse_dns_name(start, end, p);
    CHECK_DNAME(ptr->ptr_name, ptr);
    return ptr;
}

static bool parse_answer_rrs(result& res, buffer& res_buf, const char*& p, int answer)
{
    rr_base rr, *record;
    const char *start = res_buf.peek();
    const char *end   = res_buf.end();

    for (int i = 0; i < answer; i++) {
        if (!parse_rr_base(rr, start, end, p)) return false;
        switch (rr.type) {
        case A: record = parse_a_rdata(rr, p); break;
        case MX: record = parse_mx_rdata(rr, start, end, p); break;
        case NS: record = parse_ns_rdata(rr, start, end, p); break;
        case SOA: record = parse_soa_rdata(rr, start, end, p); break;
        case PTR: record = parse_ptr_rdata(rr, start, end, p); break;
        case TXT: record = parse_txt_rdata(rr, p); break;
        case CNAME: record = parse_cname_rdata(rr, start, end, p); break;
        }
        if (!record) return false;
        res.emplace_back(record);
    }
    return true;
}

static bool match_name(std::string_view name, std::string_view rname)
{
    if (rname.back() == '.') rname.remove_suffix(1);
    return name == rname;
}

// UDP will receive a complete response once.
void resolver::unpack(buffer& res_buf)
{
    const char *p = res_buf.peek();

    if (res_buf.readable() < sizeof(dns_header)) return;

    uint16_t id   = ntohs(u16(p));
    uint8_t qr    = p[0] >> 7;
    uint8_t ra    = p[1] >> 7;
    uint8_t rcode = p[1] & 0x0f;
    p += 2;

    if (!qr) return; // Not a response.
    // Almost all DNS servers support recursive queries.
    if (!ra) log_warn("(resolver) Recursion is not available");

    // Queries or their responses may be reordered by the network,
    // or by processing in name servers, so we should not depend
    // on them being returned in order.

    // Match the query.
    auto it = query_map.find(id);
    if (it == query_map.end()) return;

    auto *qc = it->second.get();
    log_info("(resolver) Received response(id=%hu)", qc->id);

    if (rcode != NoError) {
        err_notify(qc, get_rcode_str(rcode));
        return;
    }

    uint16_t qd_count = ntohs(u16(p));
    uint16_t an_count = ntohs(u16(p));
    uint16_t ns_count = ntohs(u16(p));
    uint16_t ar_count = ntohs(u16(p));

    UNUSED(ns_count);
    UNUSED(ar_count);

    // Parse question section.
    // QName(Variable) + QType(2) + QClass(2)

    if (qd_count != 1) return;

    auto name = parse_dns_name(res_buf.peek(), res_buf.end(), p);
    if (name.empty() || !match_name(qc->name, name)) return;

    if (p + 4 >= res_buf.end() || qc->q_type != ntohs(u16(p)) || qc->q_class != ntohs(u16(p))) {
        return;
    }

    result res;
    if (!parse_answer_rrs(res, res_buf, p, an_count)) return;
    if (p != res_buf.end()) return;

    notify(qc, std::move(res));
}

static const char *get_type_str(int type)
{
    switch (type) {
    case A:     return "A";
    case NS:    return "NS";
    case MX:    return "MX";
    case SOA:   return "SOA";
    case PTR:   return "PTR";
    case TXT:   return "TXT";
    case CNAME: return "CNAME";
    default: return nullptr;
    }
}

result_future resolver::query(std::string_view name, uint16_t q_type, uint16_t q_class)
{
    auto *type_str = get_type_str(q_type);
    if (!type_str || name.empty()) return result_future();

    auto q_name = to_dns_name(q_type == PTR ? to_arpa_name(name) : name);
    if (q_name.empty()) return result_future();

    auto *qc = new query_context();
    qc->name    = q_type == PTR ? to_arpa_name(name) : name;
    qc->q_name  = q_name;
    qc->q_type  = q_type;
    qc->q_class = q_class;
    // 1 + 2 + 4 + 8 == 15(s)
    qc->backoff = ExponentialBackoff(1000, 2, 4);

    auto f = qc->res_promise.get_future();

    loop->queue_in_loop([this, qc]{ do_query(qc); });

    return f;
}

static uint16_t generate_transaction_id()
{
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<uint16_t> u;
    return u(g);
}

void resolver::do_query(query_context *qc)
{
    while (true) {
        qc->id = generate_transaction_id();
        auto res = query_map.emplace(qc->id, qc);
        // There is almost no conflict.
        if (res.second) break;
    }
    qc->pack();
    log_info("(resolver) Query(name=%s, type=%s, id=%hu)", qc->name.c_str(), get_type_str(qc->q_type), qc->id);
    send_query(qc);
    set_retransmit_timer(qc);
}

#define CLASS_IN 1 // the internet

result_future resolver::query(std::string_view name, int type)
{
    return query(name, type, CLASS_IN);
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
            cout << x->name << " domain name pointer " << x->ptr_name << "\n";
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
