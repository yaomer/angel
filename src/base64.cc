#include <angel/base64.h>

namespace angel {

// Firstly, we take 3-byte as a group, and take 6-bit for calculation each time,
// which can just be represented as 4 base64-char.

// If the number of bytes to be encoded is not divisible by 3, we will complement
// zero at the back so that it can be divisible by 3, and add one or two '=' after
// the encoded base64 text to indicate the number of bytes to be supplemented.

// That is to say, if there is 1 byte left, just add 4 zeros at the end to
// represent 2 base64-char, and then add '==' after the encoded base64 text;
// there are 2 bytes left, 2 zeros are added to represent 3 base64-char,
// and then add '=' after the encoded base64 text.

// e.g.
// base64_encode("ABC") = "QUJD"
// +-----------------+-----------------+-----------------+
// |      A(65)      |      B(66)      |      C(67)      |
// +-----------------+-----------------+-----------------+
// | 0 1 0 0 0 0 0 1 | 0 1 0 0 0 0 1 0 | 0 1 0 0 0 0 1 1 |
// +------------+-------------+-------------+------------+
// |    Q(16)   |    U(20)    |     J(9)    |    D(3)    |
// +------------+-------------+-------------+------------+
//

static const char *standard_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char *url_safe_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const int max_line_limit = 76;

static const char *CRLF = "\r\n";

enum Encode { Standard, Mime, UrlSafe };

static std::string base64_encode(std::string_view data, Encode code)
{
    std::string res;

    if (data.empty()) return res;

    size_t len = data.size();
    res.reserve(len);

    const char *alphabet = (code == UrlSafe) ? url_safe_alphabet : standard_alphabet;

    unsigned char a, b, c;
    unsigned char r[4];
    size_t j = 0;
    for (size_t i = 0; i + 3 <= len; i += 3) {
        a = data[i];
        b = data[i + 1];
        c = data[i + 2];
        r[0] = alphabet[a >> 2];
        r[1] = alphabet[((a & 0x03) << 4) | (b >> 4)];
        r[2] = alphabet[((b & 0x0F) << 2) | (c >> 6)];
        r[3] = alphabet[( c & 0x3F)];
        res.append((const char*)r, 4);
        if (code == Mime) {
            j += 4;
            if (j == max_line_limit) {
                res.append(CRLF);
                j = 0;
            }
        }
    }
    switch (len % 3) {
    case 1:
        a = data[len - 1];
        r[0] = alphabet[a >> 2];
        r[1] = alphabet[((a & 0x03) << 4)];
        res.append((const char*)r, 2);
        res.append("==");
        break;
    case 2:
        a = data[len - 2];
        b = data[len - 1];
        r[0] = alphabet[a >> 2];
        r[1] = alphabet[((a & 0x03) << 4) | (b >> 4)];
        r[2] = alphabet[((b & 0x0F) << 2)];
        res.append((const char*)r, 3);
        res.append("=");
        break;
    }
    return res;
}

static constexpr unsigned char standard_mapchars[256] = {
    0,      0,      0,      0,      0,      0,      0,      0, //  1
    0,      0,      0,      0,      0,      0,      0,      0, //  2
    0,      0,      0,      0,      0,      0,      0,      0, //  3
    0,      0,      0,      0,      0,      0,      0,      0, //  4
    0,      0,      0,      0,      0,      0,      0,      0, //  5
    0,      0,      0,     62,      0,      0,      0,     63, //  6 +/
   52,     53,     54,     55,     56,     57,     58,     59, //  7 01234567
   60,     61,      0,      0,      0,      0,      0,      0, //  8 89
    0,      0,      1,      2,      3,      4,      5,      6, //  9 ABCDEFG
    7,      8,      9,     10,     11,     12,     13,     14, // 10 HIJKLMNO
   15,     16,     17,     18,     19,     20,     21,     22, // 11 PQRSTUVW
   23,     24,     25,      0,      0,      0,      0,      0, // 12 XYZ
    0,     26,     27,     28,     29,     30,     31,     32, // 13 abcdefg
   33,     34,     35,     36,     37,     38,     39,     40, // 14 hijklmno
   41,     42,     43,     44,     45,     46,     47,     48, // 15 pqrstuvw
   49,     50,     51,      0,      0,      0,      0,      0, // 16 xyz
};

static constexpr unsigned char url_safe_mapchars[256] = {
    0,      0,      0,      0,      0,      0,      0,      0, //  1
    0,      0,      0,      0,      0,      0,      0,      0, //  2
    0,      0,      0,      0,      0,      0,      0,      0, //  3
    0,      0,      0,      0,      0,      0,      0,      0, //  4
    0,      0,      0,      0,      0,      0,      0,      0, //  5
    0,      0,      0,      0,      0,     62,      0,      0, //  6 -
   52,     53,     54,     55,     56,     57,     58,     59, //  7 01234567
   60,     61,      0,      0,      0,      0,      0,      0, //  8 89
    0,      0,      1,      2,      3,      4,      5,      6, //  9 ABCDEFG
    7,      8,      9,     10,     11,     12,     13,     14, // 10 HIJKLMNO
   15,     16,     17,     18,     19,     20,     21,     22, // 11 PQRSTUVW
   23,     24,     25,      0,      0,      0,      0,     63, // 12 XYZ_
    0,     26,     27,     28,     29,     30,     31,     32, // 13 abcdefg
   33,     34,     35,     36,     37,     38,     39,     40, // 14 hijklmno
   41,     42,     43,     44,     45,     46,     47,     48, // 15 pqrstuvw
   49,     50,     51,      0,      0,      0,      0,      0, // 16 xyz
};

#define __map_char(c) (mapchars[(unsigned char)(c)])

// ascii('=') = 61 = __map_char('9')
static const unsigned char EQ = 128;

// 1) We ignore invalid characters (not in 'alphabet') in decoding.
// 2) We terminate decoding when we meet 'xx==' or 'x==='. ('x' indicates a valid base64-char)
// 3) When 'xxxx' is a valid character sequence at the end, it can also be followed by some invalid characters.
static std::string base64_decode(std::string_view data, Encode code)
{
    std::string res;

    size_t len = data.size();
    // A valid base64 text has at least 4 bytes.
    if (len < 4) return res;
    res.reserve(len * 0.75);

    auto *mapchars = (code == UrlSafe) ? url_safe_mapchars : standard_mapchars;

    unsigned char r[3];
    unsigned char c[4];
    unsigned char ch;
    size_t i = 0, j = 0;
    while (i < len) {
        ch = data[i++];
        if ((c[j] = __map_char(ch))) {
            ; // Get a valid base64-char
        } else if (ch == '=') {
            if (j < 2) continue;
            c[j] = EQ;
        } else {
            continue;
        }
        if (++j < 4) continue;
        // 'xx=x' is illegal.
        if (c[2] == EQ && c[3] != EQ) {
            c[2] = c[3];
            j--;
            continue;
        }
        j = 0;

        // xx==: 00xx xxxx 00xx 0000
        // xxx=: 00xx xxxx 00xx xxxx 00xx xx00
        // xxxx: 00xx xxxx 00xx xxxx 00xx xxxx 00xx xxxx
        r[0] = (c[0] << 2) | (c[1] >> 4);
        if (c[2] == EQ) {
            res.append((const char*)r, 1);
            return res;
        }

        r[1] = (c[1] << 4) | (c[2] >> 2);
        if (c[3] == EQ) {
            res.append((const char*)r, 2);
            return res;
        }

        r[2] = (c[2] << 6) | (c[3]);
        res.append((const char*)r, 3);
    }

    return j == 0 ? res : "";
}

std::string base64::encode(std::string_view data)
{
    return base64_encode(data, Standard);
}

std::string base64::decode(std::string_view data)
{
    return base64_decode(data, Standard);
}

std::string base64::urlsafe_encode(std::string_view data)
{
    return base64_encode(data, UrlSafe);
}

std::string base64::urlsafe_decode(std::string_view data)
{
    return base64_decode(data, UrlSafe);
}

std::string base64::mime_encode(std::string_view data)
{
    return base64_encode(data, Mime);
}

std::string base64::mime_decode(std::string_view data)
{
    return base64_decode(data, Mime);
}

}
