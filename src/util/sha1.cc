#include <string>
#include <assert.h>

#include <angel/sockops.h>

namespace angel {
namespace util {

static const uint32_t ChunkWords = 16;
static const uint32_t ChunkBytes = ChunkWords * 4;

// The conversion between Word and Byte follows the "big-endian"
// e.g. Convert "ABCD" to a Word(32-bits):
//
// 0100 0001 0100 0010 0100 0011 0100 0100
// ---------+---------+---------+---------
//     A         B         C         D
//          high    <-   low
//

static void padding(std::string& buf, std::string_view& data)
{
    // Padding bit '1' (0x80)
    // Padding bit '0' to (total_bits % 512 == 448)
    int r = ((data.size() + 1) * 8) % 512;
    int k = r <= 448 ? 0 : 1;
    int padding_zeros = (k * 512 + 448 - r) / 8;
    int padding_bytes = 1 + padding_zeros + 8;

    int remain_bytes = ChunkBytes * (k + 1) - padding_bytes;
    buf.append(data.data() + data.size() - remain_bytes, remain_bytes);

    buf.append(1, (char)0x80);

    buf.append(padding_zeros, (char)0x00);

    // Padding 64-bits(8-bytes) origin data bits
    uint64_t encoded_bits = sockops::hton64(data.size() * 8);
    buf.append(reinterpret_cast<const char*>(&encoded_bits), 8);

    data.remove_suffix(remain_bytes);
    assert(data.size() % ChunkBytes == 0);
    assert(buf.size() % ChunkBytes == 0);
}

// Rotate Left Shift
#define rls(x, n) ((x << n) | (x >> (32 - n)))

static void chunk_cal(const char *chunk, uint32_t h[5])
{
    uint32_t a = h[0];
    uint32_t b = h[1];
    uint32_t c = h[2];
    uint32_t d = h[3];
    uint32_t e = h[4];
    uint32_t f, k, t;
    uint32_t w[80];

    for (int i = 0; i < 80; i++) {
        if (i >= 0 && i <= 19) {
            f = (b & c) ^ (~b & d);
            k = 0x5A827999;
        } else if (i >= 20 && i <= 39) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i >= 40 && i <= 59) {
            f = (b & c) ^ (b & d) ^ (c & d);
            k = 0x8F1BBCDC;
        } else if (i >= 60 && i <= 79) {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        if (i <= 15) {
            // Convert chunk[4] to a Word(32-bits)
            w[i] = (chunk[4 * i + 0] & 0xff) << 24 |
                   (chunk[4 * i + 1] & 0xff) << 16 |
                   (chunk[4 * i + 2] & 0xff) << 8 |
                   (chunk[4 * i + 3] & 0xff);
        } else {
            w[i] = rls((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);
        }
        t = rls(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rls(b, 30);
        b = a;
        a = t;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

// Convert to a 20-byte string
static std::string to_digest(uint32_t h[5])
{
    uint32_t u32;
    std::string buf;
    // digest = h0 + h1 + h2 + h3 + h4;
    buf.reserve(20);
    for (int i = 0; i < 5; i++) {
        u32 = htonl(h[i]);
        buf.append(reinterpret_cast<const char*>(&u32), 4);
    }
    return buf;
}

// Convert to a 40-byte hex string
static std::string to_hex_digest(uint32_t h[5])
{
    std::string buf;
    static const char *tohex = "0123456789ABCDEF";

    buf.reserve(40);
    for (int i = 0; i < 5; i++) {
        buf.push_back(tohex[(h[i] >> 4 * 7) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 6) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 5) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 4) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 3) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 2) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 1) & 0x0f]);
        buf.push_back(tohex[(h[i] >> 4 * 0) & 0x0f]);
    }
    return buf;
}

std::string sha1(std::string_view data, bool normal)
{
    std::string buf;
    buf.reserve(ChunkBytes);
    padding(buf, data);

    uint32_t h[] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };

    while (!data.empty()) {
        chunk_cal(data.data(), h);
        data.remove_prefix(ChunkBytes);
    }
    // Calculate last one or two padding chunk(s).
    chunk_cal(buf.data(), h);
    if (buf.size() > ChunkBytes)
        chunk_cal(buf.data() + ChunkBytes, h);

    return normal ? to_digest(h) : to_hex_digest(h);
}

}
}
