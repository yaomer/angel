#ifndef __ANGEL_BASE64_H
#define __ANGEL_BASE64_H

#include <string>

namespace angel {

struct base64 {
    // Standard base64 encode and decode.
    static std::string encode(std::string_view data);
    // Return "" if decoding error.
    static std::string decode(std::string_view data);
    // Base64 Encoding with URL and Filename Safe Alphabet
    static std::string urlsafe_encode(std::string_view data);
    static std::string urlsafe_decode(std::string_view data);
    // Insert line break CRLF every 76 characters,
    // and ensure that the output always ends with CRLF.
    // (As required by rfc2045)
    static std::string mime_encode(std::string_view data);
    static std::string mime_decode(std::string_view data);
};

}

#endif // __ANGEL_BASE64_H
