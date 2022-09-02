#ifndef __HTON_H
#define __HTON_H

#include <string>

namespace detail {

// host: little-endian -> net: big-endian
// val: uint16_t, uint32_t or uint64_t
template <typename T,
         std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
void hton(std::string& buf, T val)
{
    switch (sizeof(val)) {
    case 8:
        buf.push_back((val >> 8 * 7) & 0xff);
        buf.push_back((val >> 8 * 6) & 0xff);
        buf.push_back((val >> 8 * 5) & 0xff);
        buf.push_back((val >> 8 * 4) & 0xff);
    case 4:
        buf.push_back((val >> 8 * 3) & 0xff);
        buf.push_back((val >> 8 * 2) & 0xff);
    case 2:
        buf.push_back((val >> 8 * 1) & 0xff);
        buf.push_back((val >> 8 * 0) & 0xff);
        break;
    default:
        throw std::bad_exception();
    }
}

}

#endif // __HTON_H
