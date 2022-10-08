#ifndef __ANGEL_MIME_H
#define __ANGEL_MIME_H

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>

#include <angel/util.h>

namespace angel {
namespace mime {

struct charset {
    enum { NONE, QP, BASE64, BIT7, BIT7or8 };
    static int get_encoding(std::string_view charset);
    static const char *get_canonical(std::string_view charset);
};

struct encoder {
    // QP: quoted-printable
    static std::string encode_QP(std::string_view data);
    static std::string encode_base64(std::string_view data);
    // Return "7bit" or "8bit"
    static std::string encode_7or8bit(std::string_view data);
};

struct field_type {
    field_type(const char *s) : field(s) {  }
    field_type(std::string_view s) : field(s) {  }
    std::string field;
};

inline bool operator==(const field_type& l, const field_type& r)
{
    return util::equal_case(l.field, r.field);
}

struct field_hash {
    size_t operator()(const field_type& f) const
    {
        return std::hash<std::string>()(util::to_lower(f.field));
    }
};

class base {
public:
    virtual ~base() {  }
    std::string& operator[](std::string_view field);
    void add_header(std::string_view field, std::string_view value);
    virtual std::string& str();
private:
    void init(std::string_view data,
              const char *type,
              const char *subtype,
              const char *name,
              int encoding,
              const char *charset = nullptr);
    void encode(int encoding);

    bool top_level = true;
    std::unordered_map<field_type, std::string, field_hash> headers;
    std::string_view data;
    std::string encoded_data;
    std::string buf;

    friend struct text;
    friend struct image;
    friend struct audio;
    friend struct application;
    friend class message;
    friend class multipart;
};

struct text : public base {
    // default text/plain; charset=us-ascii
    text(std::string_view txtdata, const char *name = nullptr);
    text(std::string_view txtdata, const char *subtype, const char *charset, const char *name = nullptr);
};

struct image : public base {
    image(std::string_view imgdata, const char *subtype, const char *name);
};

struct audio : public base {
    audio(std::string_view audata, const char *subtype, const char *name);
};

struct application : public base {
    // default application/octet-stream
    application(std::string_view appdata, const char *name = nullptr);
    application(std::string_view appdata, const char *subtype, const char *name = nullptr);
};

class message : public base {
public:
    message(base *message, const char *subtype = "rfc822");
    std::string& str() override;
private:
    std::unique_ptr<base> body;
    std::string data;
};

class multipart : public base {
public:
    // subtype: mixed, related, or alternative
    explicit multipart(const char *subtype = "mixed");
    multipart& attach(base *message);
    std::string& str() override;
private:
    std::vector<std::unique_ptr<base>> bodies;
    std::string data;
    std::string boundary;
};

struct unknown_charset_exception {  };

}
}

#endif // __ANGEL_MIME_H
