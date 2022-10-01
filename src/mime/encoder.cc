#include <angel/mime.h>

#include <angel/util.h>

namespace angel {
namespace mime {

// The 76 character limit does not count the trailing CRLF.
static const int max_line_limit = 76;

static inline bool is_QPchar(unsigned char c)
{
    return (c >= 33 && c <= 60) || (c >= 62 && c <= 126);
}

static inline void to_QP(std::string& res, unsigned char c)
{
    static const char *tohex = "0123456789ABCDEF";
    res.push_back('=');
    res.push_back(tohex[c >> 4]);
    res.push_back(tohex[c & 0x0f]);
}

std::string encoder::encode_QP(std::string_view data)
{
    std::string res;
    res.reserve(data.size());

    int count = 0;
    size_t n = data.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char c = data[i];
        if (count + 1 <= max_line_limit && is_QPchar(c)) {
            res.push_back(c);
            count++;
        } else if (count + 3 <= max_line_limit) {
            to_QP(res, c);
            count += 3;
        } else {
            // Convert trailing blank and tab.
            if (res.back() == ' ' || res.back() == '\t') {
                std::string tmp;
                while (res.back() == ' ' || res.back() == '\t') {
                    tmp.push_back(res.back());
                    res.pop_back();
                    count--;
                }
                while (!tmp.empty()) {
                    if (count + 3 > max_line_limit) break;
                    to_QP(res, tmp.back());
                    count += 3;
                }
                i -= tmp.size();
            }
            // Insert Soft line break
            res.append("=\r\n");
            count = 0;
            i--;
        }
    }
    return res;
}

std::string encoder::encode_base64(std::string_view data)
{
    std::string res;
    auto base64_data = util::base64_encode(data);
    size_t n = base64_data.size();
    size_t pos = 0;
    while (pos + max_line_limit <= n) {
        res.append(base64_data.data() + pos, max_line_limit);
        res.append("\r\n");
        pos += max_line_limit;
    }
    if (pos < n) {
        res.append(base64_data.data() + pos, n - pos);
        res.append("\r\n");
    }
    return res;
}

static bool decode_ascii(std::string_view data)
{
    size_t n = data.size();
    for (size_t i = 0; i < n; i++) {
        if ((unsigned char)data[i] & 0x80) return false;
    }
    return true;
}

std::string encoder::encode_7or8bit(std::string_view data)
{
    return decode_ascii(data) ? "7bit" : "8bit";
}

}
}
