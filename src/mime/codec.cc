#include <angel/mime.h>

#include <angel/util.h>
#include <angel/base64.h>

#include "qp.h"

namespace angel {
namespace mime {

std::string codec::encode_QP(std::string_view data)
{
    return QP::encode(data);
}

std::string codec::decode_QP(std::string_view data)
{
    return QP::decode(data);
}

std::string codec::encode_base64(std::string_view data)
{
    return base64::mime_encode(data);
}

std::string codec::decode_base64(std::string_view data)
{
    return base64::mime_decode(data);
}

static bool decode_ascii(std::string_view data)
{
    size_t n = data.size();
    for (size_t i = 0; i < n; i++) {
        if ((unsigned char)data[i] & 0x80) return false;
    }
    return true;
}

std::string codec::encode_7or8bit(std::string_view data)
{
    return decode_ascii(data) ? "7bit" : "8bit";
}

}
}
