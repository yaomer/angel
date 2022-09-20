#include <string>

namespace angel {
namespace util {

static const char *indexs = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//
// 首先我们以3-byte为一个分组，每次取6-bit进行计算，这刚好可以表示为4个base64字符。
//
// 如果要编码的字节数不能被3整除，我们就在后面补足0，以便其能够被3整除，
// 并在编码后的base64文本后添加一个或两个'='，表示补足的字节数。
//
// 也就是说，如果剩下1个字节，就补上4个0，以便表示为2个base64字符，再在编码后的base64文本后添加'=='；
// 剩下2个字节，就补上2个0，以便表示为3个base64字符，再在编码后的base64文本后添加'='。
//
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

#define APPEND(i) res.push_back(indexs[i])

std::string base64_encode(const char *data, size_t len)
{
    std::string res;
    res.reserve(len);
    unsigned char a, b, c;
    unsigned char i1, i2, i3, i4;
    size_t i = 0;
    while (true) {
        if (i + 3 > len) break;
        a = data[i];
        b = data[i + 1];
        c = data[i + 2];
        i1 = a >> 2;
        i2 = ((a & 0x03) << 4) | (b >> 4);
        i3 = ((b & 0x0F) << 2) | (c >> 6);
        i4 = (c & 0x3F);
        APPEND(i1);
        APPEND(i2);
        APPEND(i3);
        APPEND(i4);
        i += 3;
    }
    switch (len % 3) {
    case 1:
        a = data[len - 1];
        i1 = a >> 2;
        i2 = ((a & 0x03) << 4);
        APPEND(i1);
        APPEND(i2);
        res.append("==");
        break;
    case 2:
        a = data[len - 2];
        b = data[len - 1];
        i1 = a >> 2;
        i2 = ((a & 0x03) << 4) | (b >> 4);
        i3 = ((b & 0x0F) << 2);
        APPEND(i1);
        APPEND(i2);
        APPEND(i3);
        res.append("=");
        break;
    }
    return res;
}

#undef APPEND

static constexpr unsigned char mapchars[128] = {
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

#define APPEND(c) res.push_back(c)

std::string base64_decode(const char *data, size_t len)
{
    std::string res;
    assert(len > 0 && len % 4 == 0);
    res.reserve(len * 0.75);
    unsigned char a, b, c, d;
    size_t i = 0;
    while (true) {
        if (i + 4 > len - 4) break;
        MAP4CHAR(a, b, c, d, data[i], data[i + 1], data[i + 2], data[i + 3])
        // 00xx xxxx 00xx xxxx 00xx xxxx 00xx xxxx
        a = (a << 2) | (b >> 4);
        b = (b << 4) | (c >> 2);
        c = (c << 6) | (d);
        APPEND(a);
        APPEND(b);
        APPEND(c);
        i += 4;
    }

    MAP4CHAR(a, b, c, d, data[i], data[i + 1], data[i + 2], data[i + 3])
    // xx==: 00xx xxxx 00xx 0000
    // xxx=: 00xx xxxx 00xx xxxx 00xx xx00
    // xxxx: 00xx xxxx 00xx xxxx 00xx xxxx 00xx xxxx
    a = (a << 2) | (b >> 4);
    APPEND(a);
    if (data[i + 2] != '=') {
        b = (b << 4) | (c >> 2);
        APPEND(b);
        if (data[i + 3] != '=') {
            c = (c << 6) | (d);
            APPEND(c);
        }
    }
    return res;
}

}
}
