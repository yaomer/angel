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

std::string& message::operator[](std::string_view field)
{
    return headers[field];
}

void message::add_header(std::string_view field, std::string_view value)
{
    headers.emplace(field, value);
}

void message::encode(int encoding)
{
    switch (encoding) {
    case charset::QP:
        encoded_content = encoder.encode_QP(content);
        add_header("Content-Transfer-Encoding", "quoted-printable");
        break;
    case charset::BASE64:
        encoded_content = encoder.encode_base64(content);
        add_header("Content-Transfer-Encoding", "base64");
        break;
    case charset::BIT7:
        encoded_content = content;
        encoded_content.append("\r\n");
        add_header("Content-Transfer-Encoding", "7bit");
        break;
    case charset::BIT7or8:
        encoded_content = content;
        encoded_content.append("\r\n");
        add_header("Content-Transfer-Encoding", encoder.encode_7or8bit(content));
        break;
    }
}

std::string& message::str()
{
    if (top_level) {
        add_header("Date", format_date());
        add_header("MIME-Version", "1.0");
    }

    data.clear();
    for (auto& [field, value] : headers) {
        data.append(field.field).append(": ").append(value).append("\r\n");
    }
    data.append("\r\n");
    data.append(encoded_content);
    return data;
}

text::text(std::string_view content,
           const char *subtype,
           const char *_charset)
{
    this->content = content;

    std::string cs(str_to_lower(_charset));

    int encoding = charset.get_encoding(cs);
    if (encoding == charset::NONE) throw exception("unknown charset");

    std::string content_type("text");
    content_type.append("/").append(subtype).append("; ").append("charset=").append(cs);
    add_header("Content-Type", content_type);

    encode(encoding);
}

image::image(std::string_view img,
             const char *subtype,
             const char *name)
{
    this->content = img;
    std::string content_type("image");
    content_type.append("/").append(subtype).append("; ").append("name=").append(name);
    add_header("Content-Type", content_type);
    encode(charset::BASE64);
}

static std::string generate_boundary()
{
    static thread_local size_t part = 1;
    static thread_local std::mt19937 e(time(nullptr));
    static thread_local std::uniform_int_distribution<int> u;
    // --=_Part_{seq}_{rand-str}.{timestamp-str}
    std::ostringstream oss;
    oss << "--=_Part_" << part++ << "_" << u(e) << ".";
    oss << angel::util::get_cur_time_ms();
    return oss.str();
}

multipart::multipart(const char *subtype)
{
    boundary = generate_boundary();
    std::string content_type("multipart");
    content_type.append("/").append(subtype).append("; ")
                .append("boundary=\"").append(boundary).append("\"");
    add_header("Content-Type", content_type);
}

multipart& multipart::attach(message *msg)
{
    msg->top_level = false;
    bodies.emplace_back(msg);
    return *this;
}

std::string& multipart::str()
{
    data.clear();
    data.append(message::str());
    for (auto& body : bodies) {
        data.append("--").append(boundary).append("\r\n");
        data.append(body->str());
    }
    data.append("--").append(boundary).append("--\r\n\r\n");
    return data;
}

}
}
