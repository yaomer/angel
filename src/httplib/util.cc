#include "util.h"

#include <sys/stat.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <angel/util.h>
#include <angel/sha1.h>

namespace angel {
namespace httplib {

static constexpr const char *hexchars = "0123456789ABCDEF";

std::string uri_encode(std::string_view uri)
{
    std::string res;
    size_t n = uri.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char c = uri[i];
        if (!isalnum(c) && !strchr("-_.~", c)) {
            res.push_back('%');
            res.push_back(hexchars[c >> 4]);
            res.push_back(hexchars[c & 0x0f]);
        } else {
            res.push_back(c);
        }
    }
    return res;
}

static char from_hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    else return -1;
}

bool uri_decode(std::string_view uri, std::string& res)
{
    size_t n = uri.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char c = uri[i];
        if (c == '%') {
            char c1 = from_hex(uri[++i]);
            char c2 = from_hex(uri[++i]);
            if (c1 == -1 || c2 == -1) return false;
            res.push_back(((unsigned char)c1 << 4) | c2);
        } else {
            res.push_back(c);
        }
    }
    return true;
}

// We only support rfc1123-date, do not support rfc850-date | asctime-date
//
// Date     = wkday "," <SP> date <SP> time <SP> "GMT" ; (Fixed Length)
// date     = 2DIGIT <SP> month <SP> 4DIGIT ; (day month year)
// time     = 2DIGIT ":" 2DIGIT ":" 2DIGIT ; (00:00:00 - 23:59:59)
// wkday    = "Mon" | "Tue" | "Wed"
//          | "Thu" | "Fri" | "Sat" | "Sun"
// month    = "Jan" | "Feb" | "Mar" | "Apr"
//          | "May" | "Jun" | "Jul" | "Aug"
//          | "Sep" | "Oct" | "Nov" | "Dec"

using Clock = std::chrono::system_clock;

// e.g. Wed, 15 Nov 1995 06:25:24 GMT
static std::string format_date(const Clock::time_point& now)
{
    std::ostringstream oss;
    auto tm = Clock::to_time_t(now);
    oss << std::put_time(std::gmtime(&tm), "%a, %d %b %Y %T GMT");
    return oss.str();
}

std::string format_date()
{
    return format_date(Clock::now());
}

static const std::unordered_set<std::string_view> wkdays = {
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

static const std::unordered_map<std::string_view, int> months = {
    { "Jan",    1 },
    { "Feb",    2 },
    { "Mar",    3 },
    { "Apr",    4 },
    { "May",    5 },
    { "Jun",    6 },
    { "Jul",    7 },
    { "Aug",    8 },
    { "Sep",    9 },
    { "Oct",   10 },
    { "Nov",   11 },
    { "Dec",   12 },
};

static const int days[2][13] = {
    { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
};

static bool is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static const char SP = ' ';

// e.g. Wed, 15 Nov 1995 06:25:24 GMT
bool check_date_format(std::string_view date)
{
    const char *p = date.data();
    if (date.size() != 29) return false;

    // check wkday
    if (!wkdays.count({p, 3})) return false;
    if (p[3] != ',' || p[4] != SP) return false;
    p += 5;

    // check date (15 Nov 1995)
    if (p[2] != SP || p[6] != SP || p[11] != SP) return false;

    auto year = util::svtoi({p + 7, 4});
    if (year.value_or(-1) <= 0) return false;

    auto it = months.find({p + 3, 3});
    if (it == months.end()) return false;

    auto day = util::svtoi({p, 2});
    if (day.value_or(-1) <= 0 || day > days[is_leap(year.value())][it->second])
        return false;
    p += 12;

    // check time
    // (00:00:00 - 23:59:59)
    if (p[2] != ':' || p[5] != ':' || p[8] != SP) return false;
    // Avoid -0, although it can be correctly converted to 0, it is invalid.
    if (p[0] == '-' || p[3] == '-' || p[6] == '-') return false;
    auto h = util::svtoi({p, 2});
    if (h.value_or(-1) < 0 || h.value() > 23) return false;
    auto m = util::svtoi({p + 3, 2});
    if (m.value_or(-1) < 0 || m.value() > 59) return false;
    auto s = util::svtoi({p + 6, 2});
    if (s.value_or(-1) < 0 || s.value() > 59) return false;

    return (p[9] == 'G' && p[10] == 'M' && p[11] == 'P');
}

int64_t get_last_modified(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    return (int64_t)st.st_mtimespec.tv_sec * 1000000 + st.st_mtimespec.tv_nsec / 1000;
}

std::string format_last_modified(int64_t msecs)
{
    Clock::duration duration = std::chrono::microseconds(msecs);
    Clock::time_point point(duration);
    return format_date(point);
}

// entity-tag = [ weak ] opaque-tag
// weak       = "W/"
// opaque-tag = quoted-string
//
// A strong entity tag MUST change whenever the associated
// entity value changes in any way.
// A weak entity tag SHOULD change whenever the associated
// entity changes in a semantically significant way.
//
// e.g.
// strong ETag: "xyz"
// weak ETag: W/"xyz"
//
static bool is_strong_etag(std::string_view etag)
{
    return etag.size() >= 2 && etag.front() == '\"' && etag.back() == '\"';
}

static bool is_weak_etag(std::string_view etag)
{
    if (!util::starts_with(etag, "W/")) return false;
    etag.remove_prefix(2);
    return is_strong_etag(etag);
}

bool is_etag(std::string_view etag)
{
    return is_strong_etag(etag) || is_weak_etag(etag);
}

// The strong comparison function: in order to be considered equal,
// both validators MUST be identical in every way, and both MUST
// NOT be weak.
bool strong_etag_equal(std::string_view etag1, std::string_view etag2)
{
    if (!is_strong_etag(etag1) || !is_strong_etag(etag2)) return false;
    return etag1 == etag2;
}

// The weak comparison function: in order to be considered equal,
// both validators MUST be identical in every way, but either or
// both of them MAY be tagged as "weak" without affecting the
// result.
bool weak_etag_equal(std::string_view etag1, std::string_view etag2)
{
    if (!is_etag(etag1) || !is_etag(etag2)) return false;

    if (util::starts_with(etag1, "W/")) {
        etag1.remove_prefix(2);
    }
    if (util::starts_with(etag2, "W/")) {
        etag2.remove_prefix(2);
    }
    return etag1 == etag2;
}

// file etag = last_modified_time "-" filesize
std::string generate_file_etag(int64_t last_modified_time, off_t filesize)
{
    std::ostringstream oss;
    oss << "\"" << std::hex << last_modified_time;
    oss.unsetf(oss.hex);
    oss << "-" << filesize << "\"";
    return oss.str();
}

static thread_local sha1 sha1;

std::string generate_file_etag(const std::string& path, off_t filesize)
{
    std::string etag("\"");
    sha1.file(path);
    etag.append(sha1.hex_digest());
    etag.append("-").append(std::to_string(filesize));
    return etag.append("\"");
}

std::string generate_etag(std::string_view data)
{
    std::string etag("\"");
    sha1.update(data);
    etag.append(sha1.hex_digest());
    etag.append("-").append(std::to_string(data.size()));
    return etag.append("\"");
}

}
}
