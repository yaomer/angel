#include <angel/httplib.h>

#include <sys/stat.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace angel {
namespace httplib {

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
//

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

std::string get_last_modified(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    auto msec = st.st_mtimespec.tv_sec * 1000000 + st.st_mtimespec.tv_nsec / 1000;
    Clock::duration duration = std::chrono::microseconds(msec);
    Clock::time_point point(duration);
    return format_date(point);
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

}
}
