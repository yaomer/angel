#include <angel/mime.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>

#include <angel/util.h>

namespace angel {
namespace mime {

static charset charset;
static encoder encoder;

// Date: Sun, 21 Mar 1993 23:56:48 -0800 (PST)
std::string format_date()
{
    std::ostringstream oss;
    auto tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    oss << std::put_time(std::localtime(&tm), "%a, %d %b %Y %T %z (%Z)");
    return oss.str();
}

std::string& base::operator[](std::string_view field)
{
    return headers[field];
}

void base::add_header(std::string_view field, std::string_view value)
{
    headers.emplace(field, value);
}

void base::init(std::string_view data,
                const char *type,
                const char *subtype,
                const char *name,
                int encoding,
                const char *charset)
{
    this->data = data;

    std::string content_type(type);
    content_type.append("/").append(subtype);
    if (charset) {
        content_type.append("; charset=").append(charset);
    }
    if (name) {
        content_type.append("; name=\"").append(name).append("\"");
    }
    add_header("Content-Type", content_type);

    encode(encoding);
}

void base::encode(int encoding)
{
    switch (encoding) {
    case charset::QP:
        encoded_data = encoder.encode_QP(data);
        add_header("Content-Transfer-Encoding", "quoted-printable");
        break;
    case charset::BASE64:
        encoded_data = encoder.encode_base64(data);
        add_header("Content-Transfer-Encoding", "base64");
        break;
    case charset::BIT7:
        encoded_data = data;
        encoded_data.append("\r\n");
        add_header("Content-Transfer-Encoding", "7bit");
        break;
    case charset::BIT7or8:
        encoded_data = data;
        encoded_data.append("\r\n");
        add_header("Content-Transfer-Encoding", encoder.encode_7or8bit(data));
        break;
    }
}

std::string& base::str()
{
    if (top_level) {
        add_header("Date", format_date());
        add_header("MIME-Version", "1.0");
    }

    buf.clear();
    for (auto& [field, value] : headers) {
        buf.append(field.field).append(": ").append(value).append("\r\n");
    }
    buf.append("\r\n");
    buf.append(encoded_data);
    return buf;
}

text::text(std::string_view txtdata, const char *name)
{
    int encoding = charset.get_encoding("us-ascii");
    init(txtdata, "text", "plain", name, encoding, "us-ascii");
}

text::text(std::string_view txtdata, const char *subtype, const char *_charset, const char *name)
{
    std::string cs(str_to_lower(_charset));

    _charset = charset.get_canonical(cs);
    _charset = _charset ? _charset : cs.data();

    int encoding = charset.get_encoding(_charset);
    if (encoding == charset::NONE) throw unknown_charset_exception();

    init(txtdata, "text", subtype, name, encoding, _charset);
}

image::image(std::string_view imgdata, const char *subtype, const char *name)
{
    init(imgdata, "image", subtype, name, charset::BASE64);
}

audio::audio(std::string_view audata, const char *subtype, const char *name)
{
    init(audata, "audio", subtype, name, charset::BASE64);
}

application::application(std::string_view appdata, const char *name)
{
    init(appdata, "application", "octet-stream", name, charset::BASE64);
}

application::application(std::string_view appdata, const char *subtype, const char *name)
{
    init(appdata, "application", subtype, name, charset::BASE64);
}

message::message(base *message, const char *subtype)
{
    body.reset(message);
    std::string content_type("message/");
    content_type.append(subtype);
    add_header("Content-Type", content_type);
}

std::string& message::str()
{
    data.clear();
    data.append(base::str());
    data.append(body->str());
    return data;
}

static std::string generate_boundary()
{
    static thread_local size_t part = 1;
    static thread_local std::mt19937 e(time(nullptr));
    static thread_local std::uniform_int_distribution<int> u;
    // --=_Part_{seq}_{rand-str}.{timestamp-str}
    std::ostringstream oss;
    oss << "--=_Part_" << part++ << "_" << u(e) << ".";
    oss << util::get_cur_time_ms();
    return oss.str();
}

multipart::multipart(const char *subtype)
{
    boundary = generate_boundary();
    std::string content_type("multipart/");
    content_type.append(subtype).append("; boundary=\"").append(boundary).append("\"");
    add_header("Content-Type", content_type);
}

multipart& multipart::attach(base *message)
{
    message->top_level = false;
    bodies.emplace_back(message);
    return *this;
}

std::string& multipart::str()
{
    data.clear();
    data.append(base::str());
    for (auto& body : bodies) {
        data.append("--").append(boundary).append("\r\n");
        data.append(body->str());
    }
    data.append("--").append(boundary).append("--\r\n\r\n");
    return data;
}

}
}
