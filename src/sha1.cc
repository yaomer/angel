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

static void __chunk_cal(const char *chunk, uint32_t h[5])
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
    std::string buf;
    static const char *tohex = "0123456789ABCDEF";

    final_cal();
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
    init();
    return buf;
}

}
