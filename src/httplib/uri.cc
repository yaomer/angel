#include <angel/httplib.h>

namespace angel {
namespace httplib {

static constexpr const char *hexchars = "0123456789ABCDEF";

std::string uri::encode(std::string_view uri)
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

std::string uri::decode(std::string_view uri)
{
    std::string res;
    size_t n = uri.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char c = uri[i];
        if (c == '%') {
            unsigned char c1 = from_hex(uri[++i]);
            unsigned char c2 = from_hex(uri[++i]);
            res.push_back((c1 << 4) | c2);
        } else {
            res.push_back(c);
        }
    }
    return res;
}

}
}
