#include <angel/base64.h>

namespace angel {

static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

static const int max_line_limit = 76;

static const char *CRLF = "\r\n";
static const char CR = '\r';
static const char LF = '\n';

std::string base64::encode(std::string_view data)
{
    std::string res;
    unsigned char a, b, c;
    unsigned char r[4];

    if (data.empty()) return res;

    size_t len = data.size();
    res.reserve(len);

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
        if (mime) {
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
        j += 2;
        break;
    case 2:
        a = data[len - 2];
        b = data[len - 1];
        r[0] = alphabet[a >> 2];
        r[1] = alphabet[((a & 0x03) << 4) | (b >> 4)];
        r[2] = alphabet[((b & 0x0F) << 2)];
        res.append((const char*)r, 3);
        res.append("=");
        j += 3;
        break;
    }
    if (mime) {
        if (j > 0) res.append(CRLF);
    }
    mime = false;
    return res;
}

std::string base64::encode_mime(std::string_view data)
{
    mime = true;
    return encode(data);
}

static constexpr unsigned char mapchars[256] = {
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

#define MAP4CHAR(a, b, c, d, c1, c2, c3, c4) \
    a = mapchars[(unsigned char)(c1)]; \
    b = mapchars[(unsigned char)(c2)]; \
    c = mapchars[(unsigned char)(c3)]; \
    d = mapchars[(unsigned char)(c4)];

std::string base64::decode(std::string_view data)
{
    std::string res;
    unsigned char a, b, c, d;
    unsigned char r[3];

    size_t len = data.size();
    // A valid base64 text has at least 4 bytes.
    if (len < 4 || len % 4 != 0) return res;
    res.reserve(len * 0.75);

    size_t i = 0;
    while (i + 4 <= len - 4) {
        MAP4CHAR(a, b, c, d, data[i], data[i + 1], data[i + 2], data[i + 3])
        // 00xx xxxx 00xx xxxx 00xx xxxx 00xx xxxx
        r[0] = (a << 2) | (b >> 4);
        r[1] = (b << 4) | (c >> 2);
        r[2] = (c << 6) | (d);
        res.append((const char*)r, 3);
        i += 4;
    }

    MAP4CHAR(a, b, c, d, data[i], data[i + 1], data[i + 2], data[i + 3])
    // xx==: 00xx xxxx 00xx 0000
    // xxx=: 00xx xxxx 00xx xxxx 00xx xx00
    // xxxx: 00xx xxxx 00xx xxxx 00xx xxxx 00xx xxxx
    res.push_back((a << 2) | (b >> 4));
    if (data[i + 2] != '=') {
        res.push_back((b << 4) | (c >> 2));
        if (data[i + 3] != '=') {
            res.push_back((c << 6) | (d));
        }
    }
    return res;
}

std::string base64::decode_mime(std::string_view data)
{
    std::string res;
    unsigned char a, b, c, d;
    unsigned char r[3];

    size_t len = data.size();
    // A valid mime base64 text has at least 4 + 2(CRLF) bytes.
    if (len < 6) return res;
    auto remain = len % (max_line_limit + 2);
    if (remain != 0 && remain % 4 != 2) return res;
    res.reserve(len * 0.75);

    size_t i = 0, j = 0;
    while (i + 4 <= len - 6) {
        MAP4CHAR(a, b, c, d, data[i], data[i + 1], data[i + 2], data[i + 3])
        r[0] = (a << 2) | (b >> 4);
        r[1] = (b << 4) | (c >> 2);
        r[2] = (c << 6) | (d);
        res.append((const char*)r, 3);
        i += 4;
        j += 4;
        if (j == max_line_limit) {
            if (data[i] != CR || data[i + 1] != LF) return "";
            i += 2;
            j = 0;
        }
    }

    MAP4CHAR(a, b, c, d, data[i], data[i + 1], data[i + 2], data[i + 3])
    res.push_back((a << 2) | (b >> 4));
    if (data[i + 2] != '=') {
        res.push_back((b << 4) | (c >> 2));
        if (data[i + 3] != '=') {
            res.push_back((c << 6) | (d));
        }
    }

    if (data[i + 4] != CR || data[i + 5] != LF) {
        // Must end with CRLF.
        return "";
    }
    return res;
}

}
