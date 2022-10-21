#include <angel/mime.h>

#include <angel/util.h>
#include <angel/base64.h>

namespace angel {
namespace mime {

static const char *CRLF = "\r\n";

// The 76 character limit does not count the trailing CRLF.
static const int max_line_limit = 76;

static inline bool is_QPchar(unsigned char c)
{
    return (c >= 33 && c <= 60) || (c >= 62 && c <= 126) || c == ' ' || c == '\t';
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
    unsigned char c;
    for (size_t i = 0; i < n; i++) {
        c = data[i];
        // max_line_limit count "=" of "Soft line break"
        if (count + 1 <= max_line_limit - 1 && is_QPchar(c)) {
            res.push_back(c);
            count++;
        } else if (count + 3 <= max_line_limit - 1) {
            to_QP(res, c);
            count += 3;
        } else {
            // Insert Soft line break
            res.append("=").append(CRLF);
            count = 0;
            i--;
        }
    }
    // The space or tab at the end of the encoded line
    // should be represented as =20 or =09.
    c = res.back();
    res.pop_back();
    if (c == ' ' || c == '\t') {
        if (count - 1 + 3 > max_line_limit) {
            res.append("=").append(CRLF);
        }
        to_QP(res, c);
    }
    res.append(CRLF);
    return res;
}

std::string encoder::encode_base64(std::string_view data)
{
    static thread_local base64 base64;
    return base64.encode_mime(data);
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
