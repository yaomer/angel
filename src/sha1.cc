#include <angel/sha1.h>

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <angel/sockops.h>
#include <angel/util.h>

namespace angel {

static const uint32_t ChunkWords = 16;
static const uint32_t ChunkBytes = ChunkWords * 4;

// The conversion between Word and Byte follows the "big-endian"
// e.g. Convert "ABCD" to a Word(32-bit):
//
// 0100 0001 0100 0010 0100 0011 0100 0100
// ---------+---------+---------+---------
//     A         B         C         D
//          high    <-   low
//

// Copy the data with less than 512 bits at the end to the head of
// buf, and form one or two new chunk(s) with padding bytes.
static void padding(std::string& buf, const char *endptr, size_t size)
{
    // Padding bit '1' (0x80)
    // Padding bit '0' to (total_bits % 512 == 448)
    int r = ((size + 1) * 8) % 512;
    int k = r <= 448 ? 0 : 1;
    int padding_zeros = (k * 512 + 448 - r) / 8;
    int padding_bytes = 1 + padding_zeros + 8;

    int remain_bytes = ChunkBytes * (k + 1) - padding_bytes;
    buf.append(endptr - remain_bytes, remain_bytes);

    buf.append(1, (char)0x80);

    buf.append(padding_zeros, (char)0x00);

    // Padding 64-bit(8-byte) origin data bits
    uint64_t encoded_bits = sockops::hton64(size * 8);
    buf.append(reinterpret_cast<const char*>(&encoded_bits), 8);

    assert(buf.size() % ChunkBytes == 0);
}

// Rotate Left Shift
#define rls(x, n) ((x << n) | (x >> (32 - n)))

#define round1(i, k) \
    f = (b & c) ^ (~b & d); \
    w[i] = (chunk[4 * i + 0] & 0xff) << 24 | \
           (chunk[4 * i + 1] & 0xff) << 16 | \
           (chunk[4 * i + 2] & 0xff) << 8 | \
           (chunk[4 * i + 3] & 0xff); \
    t = rls(a, 5) + f + e + k + w[i]; \
    e = d; \
    d = c; \
    c = rls(b, 30); \
    b = a; \
    a = t;

#define round2(i, k) \
    f = (b & c) ^ (~b & d); \
    w[i] = rls((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1); \
    t = rls(a, 5) + f + e + k + w[i]; \
    e = d; \
    d = c; \
    c = rls(b, 30); \
    b = a; \
    a = t;

#define round3(i, k) \
    f = b ^ c ^ d; \
    w[i] = rls((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1); \
    t = rls(a, 5) + f + e + k + w[i]; \
    e = d; \
    d = c; \
    c = rls(b, 30); \
    b = a; \
    a = t;

#define round4(i, k) \
    f = (b & c) ^ (b & d) ^ (c & d); \
    w[i] = rls((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1); \
    t = rls(a, 5) + f + e + k + w[i]; \
    e = d; \
    d = c; \
    c = rls(b, 30); \
    b = a; \
    a = t;

#define round5(i, k) \
    f = b ^ c ^ d; \
    w[i] = rls((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1); \
    t = rls(a, 5) + f + e + k + w[i]; \
    e = d; \
    d = c; \
    c = rls(b, 30); \
    b = a; \
    a = t;

static void __chunk_cal(const char *chunk, uint32_t h[5])
{
    uint32_t a = h[0];
    uint32_t b = h[1];
    uint32_t c = h[2];
    uint32_t d = h[3];
    uint32_t e = h[4];
    uint32_t f, t;
    uint32_t w[80];

    round1(0,  0x5A827999)
    round1(1,  0x5A827999)
    round1(2,  0x5A827999)
    round1(3,  0x5A827999)
    round1(4,  0x5A827999)
    round1(5,  0x5A827999)
    round1(6,  0x5A827999)
    round1(7,  0x5A827999)
    round1(8,  0x5A827999)
    round1(9,  0x5A827999)
    round1(10, 0x5A827999)
    round1(11, 0x5A827999)
    round1(12, 0x5A827999)
    round1(13, 0x5A827999)
    round1(14, 0x5A827999)
    round1(15, 0x5A827999)
    round2(16, 0x5A827999)
    round2(17, 0x5A827999)
    round2(18, 0x5A827999)
    round2(19, 0x5A827999)
    round3(20, 0x6ED9EBA1)
    round3(21, 0x6ED9EBA1)
    round3(22, 0x6ED9EBA1)
    round3(23, 0x6ED9EBA1)
    round3(24, 0x6ED9EBA1)
    round3(25, 0x6ED9EBA1)
    round3(26, 0x6ED9EBA1)
    round3(27, 0x6ED9EBA1)
    round3(28, 0x6ED9EBA1)
    round3(29, 0x6ED9EBA1)
    round3(30, 0x6ED9EBA1)
    round3(31, 0x6ED9EBA1)
    round3(32, 0x6ED9EBA1)
    round3(33, 0x6ED9EBA1)
    round3(34, 0x6ED9EBA1)
    round3(35, 0x6ED9EBA1)
    round3(36, 0x6ED9EBA1)
    round3(37, 0x6ED9EBA1)
    round3(38, 0x6ED9EBA1)
    round3(39, 0x6ED9EBA1)
    round4(40, 0x8F1BBCDC)
    round4(41, 0x8F1BBCDC)
    round4(42, 0x8F1BBCDC)
    round4(43, 0x8F1BBCDC)
    round4(44, 0x8F1BBCDC)
    round4(45, 0x8F1BBCDC)
    round4(46, 0x8F1BBCDC)
    round4(47, 0x8F1BBCDC)
    round4(48, 0x8F1BBCDC)
    round4(49, 0x8F1BBCDC)
    round4(50, 0x8F1BBCDC)
    round4(51, 0x8F1BBCDC)
    round4(52, 0x8F1BBCDC)
    round4(53, 0x8F1BBCDC)
    round4(54, 0x8F1BBCDC)
    round4(55, 0x8F1BBCDC)
    round4(56, 0x8F1BBCDC)
    round4(57, 0x8F1BBCDC)
    round4(58, 0x8F1BBCDC)
    round4(59, 0x8F1BBCDC)
    round5(60, 0xCA62C1D6)
    round5(61, 0xCA62C1D6)
    round5(62, 0xCA62C1D6)
    round5(63, 0xCA62C1D6)
    round5(64, 0xCA62C1D6)
    round5(65, 0xCA62C1D6)
    round5(66, 0xCA62C1D6)
    round5(67, 0xCA62C1D6)
    round5(68, 0xCA62C1D6)
    round5(69, 0xCA62C1D6)
    round5(70, 0xCA62C1D6)
    round5(71, 0xCA62C1D6)
    round5(72, 0xCA62C1D6)
    round5(73, 0xCA62C1D6)
    round5(74, 0xCA62C1D6)
    round5(75, 0xCA62C1D6)
    round5(76, 0xCA62C1D6)
    round5(77, 0xCA62C1D6)
    round5(78, 0xCA62C1D6)
    round5(79, 0xCA62C1D6)

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

//======================================================
//======================== SHA1 ========================
//======================================================

sha1::sha1()
{
    init();
}

sha1::sha1(std::string_view data)
{
    init();
    update(data);
}

void sha1::init()
{
    h[0] = 0x67452301;
    h[1] = 0xEFCDAB89;
    h[2] = 0x98BADCFE;
    h[3] = 0x10325476;
    h[4] = 0xC3D2E1F0;
    payload = 0;
}

void sha1::chunk_cal(const char *data, size_t chunks)
{
    for (size_t i = 0; i < chunks; i++) {
        __chunk_cal(data + ChunkBytes * i, h);
    }
}

// Calculate the last remaining bytes.
void sha1::final_cal()
{
    std::string padding_buf;
    padding_buf.reserve(ChunkBytes);

    padding(padding_buf, buf.data() + buf.size(), payload);
    chunk_cal(padding_buf.data(), padding_buf.size() / ChunkBytes);
    buf.clear();
}

void sha1::update(std::string_view data)
{
    if (!buf.empty()) {
        assert(buf.size() < ChunkBytes);
        size_t len = std::min(ChunkBytes - buf.size(), data.size());
        buf.append(data.data(), len);
        data.remove_prefix(len);
        if (buf.size() == ChunkBytes) {
            chunk_cal(buf.data(), 1);
            buf.clear();
        }
    }

    size_t chunks = data.size() / ChunkBytes;
    size_t remain_bytes = data.size() % ChunkBytes;

    chunk_cal(data.data(), chunks);
    if (remain_bytes > 0) {
        buf.append(data.end() - remain_bytes, remain_bytes);
    }
    payload += data.size();
}

bool sha1::file(const std::string& path)
{
    static thread_local char fbuf[ChunkBytes * 256]; // 16K

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    while (true) {
        ssize_t n = util::read_file(fd, fbuf, sizeof(fbuf));
        if (n > 0) {
            update({fbuf, (size_t)n});
        } else if (n == 0) {
            break;
        } else {
            close(fd);
            return false;
        }
    }
    close(fd);
    return true;
}

std::string sha1::digest()
{
    uint32_t u32;
    std::string buf;
    // digest = h0 + h1 + h2 + h3 + h4;
    final_cal();
    buf.reserve(20);
    for (int i = 0; i < 5; i++) {
        u32 = htonl(h[i]);
        buf.append(reinterpret_cast<const char*>(&u32), 4);
    }
    init();
    return buf;
}

std::string sha1::hex_digest()
{
    static const char *tohex = "0123456789ABCDEF";

    std::string hexs;
    hexs.resize(40);

    final_cal();
    for (int i = 0; i < 5; i++) {
        hexs[0 + i * 8] = (tohex[(h[i] >> 4 * 7) & 0x0f]);
        hexs[1 + i * 8] = (tohex[(h[i] >> 4 * 6) & 0x0f]);
        hexs[2 + i * 8] = (tohex[(h[i] >> 4 * 5) & 0x0f]);
        hexs[3 + i * 8] = (tohex[(h[i] >> 4 * 4) & 0x0f]);
        hexs[4 + i * 8] = (tohex[(h[i] >> 4 * 3) & 0x0f]);
        hexs[5 + i * 8] = (tohex[(h[i] >> 4 * 2) & 0x0f]);
        hexs[6 + i * 8] = (tohex[(h[i] >> 4 * 1) & 0x0f]);
        hexs[7 + i * 8] = (tohex[(h[i] >> 4 * 0) & 0x0f]);
    }
    init();
    return hexs;
}

}
