#include "qp.h"

#include <angel/mime.h>

namespace angel {
namespace mime {

// quoted-printable := qp-line *(CRLF qp-line)
//
// qp-line := *(qp-segment transport-padding CRLF)
//            qp-part transport-padding
//
// qp-segment := qp-section *(SPACE / TAB) "="
//               ; Maximum length of 76 characters
//
// qp-part := qp-section
//            ; Maximum length of 76 characters
//
// qp-section := [*(ptext / SPACE / TAB) ptext]
//
// ptext := hex-octet / safe-char
//
// safe-char := <any octet with decimal value of 33 through
//              60 inclusive, and 62 through 126>
//              ; Characters not listed as "mail-safe" in
//              ; RFC 2049 are also not recommended.
//
// hex-octet := "=" 2(DIGIT / "A" / "B" / "C" / "D" / "E" / "F")
//              ; Octet must be used for characters > 127, =,
//              ; SPACEs or TABs at the ends of lines, and is
//              ; recommended for any character not listed in
//              ; RFC 2049 as "mail-safe".
//
// transport-padding := *LWSP-char
//                      ; Composers MUST NOT generate
//                      ; non-zero length transport
//                      ; padding, but receivers MUST
//                      ; be able to handle padding
//                      ; added by message transports.

static const char SP = ' ';
static const char HT = '\t';
static const char CR = '\r';
static const char LF = '\n';
static const char *CRLF = "\r\n";

// The 76 character limit does not count the trailing CRLF.
static const int max_line_limit = 76;

static inline bool is_QPchar(unsigned char c)
{
    return (c >= 33 && c <= 60) || (c >= 62 && c <= 126) || c == SP || c == HT;
}

std::string QP::encode(std::string_view data)
{
    std::string res;

    if (data.empty()) return res;

    int count = 0;
    unsigned char c;
    size_t n = data.size();
    res.reserve(n);

    for (size_t i = 0; i < n; i++) {
        c = data[i];
        // max_line_limit count "=" of "Soft line break"
        if (count + 1 <= max_line_limit - 1 && is_QPchar(c)) {
            res.push_back(c);
            count++;
        } else if (count + 3 <= max_line_limit - 1) {
            codec::to_hex_pair(res, '=', c);
            count += 3;
        } else {
            // Insert Soft line break
            res.append("=").append(CRLF);
            count = 0;
            i--;
        }
    }
    // The space or tab at the end of the encoded line
    // should be represented as =20 or =09.
    c = res.back();
    if (c == SP || c == HT) {
        res.pop_back();
        if (count - 1 + 3 > max_line_limit) {
            res.append("=").append(CRLF);
        }
        codec::to_hex_pair(res, '=', c);
    }
    return res;
}

// When decoding, the maximum length of the encoded line is
// not limited to 76 characters.
std::string QP::decode(std::string_view data)
{
    std::string res;
    unsigned char c, c1, c2;

    size_t len = data.size();
    for (size_t i = 0; i < len; i++) {
        c = data[i];
        if (c != '=') {
            res.push_back(c);
            continue;
        }

        if (++i >= len) return res;
        c1 = data[i];

        if (!ishexnumber(c1) && c1 != CR) {
            // This case is illegal.
            // We include the "=" character and the following character
            // in the decoded data without any transformation.
            res.push_back('=');
            res.push_back(c1);
            continue;
        }

        if (++i >= len) {
            res.push_back('=');
            res.push_back(c1);
            return res;
        }
        c2 = data[i];

        if (ishexnumber(c1)) { // =2(hex)
            if (ishexnumber(c2)) {
                c = codec::from_hex_pair(c1, c2);
                res.push_back(c);
                continue;
            }
        } else if (c2 == LF) { // =CRLF
            continue;
        }
        res.push_back('=');
        res.push_back(c1);
    }
    return res;
}

}
}
