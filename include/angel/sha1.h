#ifndef __ANGEL_SHA1_H
#define __ANGEL_SHA1_H

#include <string>

namespace angel {

class sha1 {
public:
    sha1();
    // Construct sha1() and call update(data).
    explicit sha1(std::string_view data);
    // If return false, there is an error in opening or reading the file.
    bool file(const std::string& path);
    // Stream interface.
    // Allow to calculate partial data at a time without buffering whole data.
    void update(std::string_view data);
    // 20-byte digest
    std::string digest();
    // 40-byte hex digest
    std::string hex_digest();
private:
    void init();
    void chunk_cal(const char *data, size_t chunks);
    void final_cal();

    uint32_t h[5];
    size_t payload;
    std::string buf;
};

}

#endif // __ANGEL_SHA1_H
