#include <string>

namespace angel {
namespace util {

#define APPEND(i) res.push_back(indexs[i])

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

}
}
