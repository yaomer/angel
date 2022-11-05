#ifndef __ANGEL_MIME_QP_H
#define __ANGEL_MIME_QP_H

#include <string>

namespace angel {
namespace mime {

// QP: quoted-printable
struct QP {
    static std::string encode(std::string_view data);
    static std::string decode(std::string_view data);
};

}
}

#endif // __ANGEL_MIME_QP_H
