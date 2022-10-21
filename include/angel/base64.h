#ifndef __ANGEL_BASE64_H
#define __ANGEL_BASE64_H

#include <string>

namespace angel {

class base64 {
public:
    // Standard base64 encode and decode.
    std::string encode(std::string_view data);
    std::string decode(std::string_view data);
    // Insert line break CRLF every 76 characters,
    // and ensure that the output always ends with CRLF.
    // (As required by rfc2045)
    std::string encode_mime(std::string_view data);
private:
    bool mime = false;
};

}

#endif // __ANGEL_BASE64_H
