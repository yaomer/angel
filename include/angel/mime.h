#ifndef __ANGEL_MIME_H
#define __ANGEL_MIME_H

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>

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

inline std::string str_to_lower(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

inline bool equal_case(std::string_view s1, std::string_view s2)
{
    return s1.size() == s2.size() && strcasecmp(s1.data(), s2.data()) == 0;
}

struct field_type {
    field_type(const char *s) : field(s) {  }
    field_type(std::string_view s) : field(s) {  }
    std::string field;
};

inline bool operator==(const field_type& l, const field_type& r)
{
    return equal_case(l.field, r.field);
}

struct field_hash {
    size_t operator()(const field_type& f) const
    {
        return std::hash<std::string>()(str_to_lower(f.field));
    }
};

class message {
public:
    virtual ~message() {  }
    std::string& operator[](std::string_view field);
    void add_header(std::string_view field, std::string_view value);
    virtual std::string& str();
private:
    void encode(int encoding);

    bool top_level = true;
    std::unordered_map<field_type, std::string, field_hash> headers;
    std::string content;
    std::string data;

    friend struct text;
    friend struct image;
    friend class multipart;
};

struct text : public message {
    text(std::string_view content,
         const char *subtype = "plain",
         const char *charset = "us-ascii");
};

struct image : public message {
    image(std::string_view img,
          const char *subtype,
          const char *name);
};

class multipart : public message {
public:
    // subtype: mixed, related, or alternative
    explicit multipart(const char *subtype = "mixed");
    multipart& attach(message *msg);
    std::string& str() override;
private:
    std::vector<std::unique_ptr<message>> bodies;
    std::string data;
    std::string boundary;
};

class exception {
public:
    explicit exception(const char *msg) : msg(msg) {  }
    const char *what() const { return msg.c_str(); }
private:
    std::string msg;
};

}
}

#endif // __ANGEL_MIME_H
