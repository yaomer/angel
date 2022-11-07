#ifndef __ANGEL_MIME_H
#define __ANGEL_MIME_H

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>

#include <angel/util.h>
#include <angel/insensitive_unordered_map.h>
#include <angel/buffer.h>

namespace angel {
namespace mime {

struct charset {
    enum { NONE, QP, BASE64, BIT7, BIT7or8 };
    static int get_encoding(std::string_view charset);
    static const char *get_canonical(std::string_view charset);
};

typedef insensitive_unordered_map<std::string> Headers;
typedef insensitive_unordered_map<std::string> Parameters;

enum ParseCode {
    NotEnough, Error, Ok,
};

struct codec {
    static void to_hex_pair(std::string& buf, char leading_char, unsigned char c)
    {
        static const char *tohexchar = "0123456789ABCDEF";
        char hexs[3] = { leading_char, tohexchar[c >> 4], tohexchar[c & 0x0f] };
        buf.append(hexs, 3);
    }

    static char to_hex_value(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        else return -1;
    }

    static char from_hex_pair(char c1, char c2)
    {
        return ((unsigned char)to_hex_value(c1) << 4) | to_hex_value(c2);
    }

    // QP: quoted-printable
    static std::string encode_QP(std::string_view data);
    static std::string decode_QP(std::string_view data);

    static std::string encode_base64(std::string_view data);
    static std::string decode_base64(std::string_view data);

    // Return "7bit" or "8bit"
    static std::string encode_7or8bit(std::string_view data);

    // Parse MIME-headers.
    static ParseCode parse_header(Headers& headers, buffer& buf);
    // Point to the closed <"> if successful, nullptr otherwise.
    static const char *parse_quoted_string(const char *first, const char *last);
};

class base {
public:
    base() {  }
    virtual ~base() {  }
    base(const base&) = delete;
    base& operator=(const base&) = delete;

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
    void encode(std::string_view data, int encoding);

    bool top_level = true;
    Headers headers;
    std::string encoded_data;
    std::string buf;

    friend struct text;
    friend struct image;
    friend struct audio;
    friend struct video;
    friend struct application;
    friend class message;
    friend class multipart;
    friend class form_data_writer;
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

struct video : public base {
    video(std::string_view vidata, const char *subtype, const char *name);
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

class multipart;

// build HTTP multipart/form-data
class form_data_writer {
public:
    explicit form_data_writer(multipart *);
    void add_text_part(std::string_view name, std::string_view value);
    void add_file_part(std::string_view name, const std::string& pathname);
    std::string get_content_type();
private:
    multipart *multipart;
};

class multipart : public base {
public:
    // subtype: mixed, related, or alternative
    explicit multipart(std::string_view subtype = "mixed");
    multipart& attach(base *part);
    std::string& str() override;
    const std::string& get_boundary() const { return boundary; }
    form_data_writer new_form_data_writer();
private:
    std::vector<std::unique_ptr<base>> parts;
    std::string boundary;
    std::string data;
};

// Parse such header:
//
// content := "Content-Field" ":" value
//            *(";" parameter)
// parameter := attribute "=" value
//
// Typical such as Content-Type, Content-Disposition
struct content_value_t {
    std::string value;
    Parameters parameters;
    bool parse(std::string_view str);
};

struct content_type_t {
    std::string_view type() { return value.value; }
    std::string_view maintype() { return util::split(value.value, '/')[0]; }
    std::string_view subtype() { return util::split(value.value, '/')[1]; }
    const Parameters& parameters() { return value.parameters; }
    bool parse(std::string_view str)
    {
        return value.parse(str) && util::split(value.value, '/').size() == 2;
    }
    content_value_t value;
};

class part_node;
class multipart_node;

// A MIME Multipart Stream Parser.
// Designed to read and parse MIME Message from nonblocking socket.
class multipart_parser {
public:
    explicit multipart_parser(std::string_view content_type);

    multipart_parser(const multipart_parser&) = delete;
    multipart_parser& operator=(const multipart_parser&) = delete;

    // When NotEnough is returned, you should continue to call when
    // there is new data, until Ok or Error is returned.
    ParseCode parse(buffer& buf);
    const std::string& get_preamble() const { return preamble; }
    const std::string& get_epilogue() const { return epilogue; }
    // Return the root of MIME message tree.
    // You can use it to iterate all parts when parse() returns Ok.
    part_node *get_root();
private:
    enum ParseState { Preamble, MultipartBody, Epilogue };
    ParseState state;
    std::string content_type;
    std::string preamble;
    std::string epilogue;
    std::unique_ptr<multipart_node> root;
};

typedef std::vector<part_node*> iter_part_list;

class part_node {
public:
    virtual ~part_node() {  }
    virtual bool is_multipart() { return false; }
    virtual iter_part_list iter_parts();
    content_type_t& get_content_type() { return content_type; }
    const Headers& get_headers() { return headers; }
    const std::string& get_body() { return body; }
private:
    bool decode(std::string_view data);

    Headers headers;
    content_type_t content_type;
    std::string body;
    friend class multipart_node;
    friend class multipart_parser;
};

class multipart_node : public part_node {
public:
    explicit multipart_node(std::string_view boundary);
    bool is_multipart() override { return true; }
    iter_part_list iter_parts() override;
private:
    enum ParseState {
        DashBoundary,
        HeaderPart,
        BodyPart,
        Delimiter,
    };

    ParseCode parse(buffer& buf);
    part_node *get_part_node(Headers& headers);

    ParseState state = DashBoundary;
    std::vector<std::unique_ptr<part_node>> parts;
    std::string dash_boundary;
    std::string delimiter;
    Headers part_headers;
    part_node *cur = nullptr;
    friend class multipart_parser;
};

std::string generate_boundary();

class mimetypes {
public:
    // Load default "angel/mime.types".
    mimetypes();
    // Load new "mime.types" configuration file.
    // The existing items in the default "angel/mime.types" will be overwritten.
    void load(const std::string& path);
    const char *get_mime_type(const std::string& filename) const;
    const std::vector<std::string> *get_file_extensions(const std::string& mime_type) const;
private:
    // Map Internet media types to unique file extension(s).
    std::unordered_map<std::string, std::vector<std::string>> mime_types;
    // Map unique file extension(s) to Internet media types.
    std::unordered_multimap<std::string, std::string> file_extensions;
};

}
}

#endif // __ANGEL_MIME_H
