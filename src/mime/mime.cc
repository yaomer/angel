//
// MIME (Multipurpose Internet Mail Extensions)
//
// See the following RFCs:
// https://www.rfc-editor.org/rfc/rfc822.html
// https://www.rfc-editor.org/rfc/rfc2045.html
// https://www.rfc-editor.org/rfc/rfc2046.html
// https://www.rfc-editor.org/rfc/rfc2047.html
// https://www.rfc-editor.org/rfc/rfc2048.html
// https://www.rfc-editor.org/rfc/rfc2049.html
// https://www.rfc-editor.org/rfc/rfc2112.html (multipart/related)
// https://www.rfc-editor.org/rfc/rfc2388.html (multipart/form-data)
//

#include <angel/mime.h>

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <sstream>

#include <angel/buffer.h>

namespace angel {
namespace mime {

static charset charset;
static mimetypes mimetypes;

static const char SP = ' ';
static const char HT = '\t';
static const char *CRLF = "\r\n";
static const char *TWO_HYPHEN = "--";

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
    std::string content_type(type);
    content_type.append("/").append(subtype);
    if (charset) {
        content_type.append("; charset=").append(charset);
    }
    if (name) {
        content_type.append("; name=\"").append(name).append("\"");
    }
    add_header("Content-Type", content_type);

    encode(data, encoding);
}

void base::encode(std::string_view data, int encoding)
{
    switch (encoding) {
    case charset::QP:
        encoded_data = codec::encode_QP(data);
        add_header("Content-Transfer-Encoding", "quoted-printable");
        break;
    case charset::BASE64:
        encoded_data = codec::encode_base64(data);
        add_header("Content-Transfer-Encoding", "base64");
        break;
    case charset::BIT7:
        encoded_data = data;
        add_header("Content-Transfer-Encoding", "7bit");
        break;
    case charset::BIT7or8:
        encoded_data = data;
        add_header("Content-Transfer-Encoding", codec::encode_7or8bit(data));
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
        buf.append(field.key).append(": ").append(value).append(CRLF);
    }
    buf.append(CRLF);
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
    std::string cs(util::to_lower(_charset));

    _charset = charset.get_canonical(cs);
    _charset = _charset ? _charset : cs.data();

    int encoding = charset.get_encoding(_charset);
    assert(encoding != charset::NONE);

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

video::video(std::string_view vidata, const char *subtype, const char *name)
{
    init(vidata, "video", subtype, name, charset::BASE64);
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

// boundary := 0*69<bchars> bcharsnospace
// bchars := bcharsnospace / " "
// bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
//                  "+" / "_" / "," / "-" / "." /
//                  "/" / ":" / "=" / "?"
std::string generate_boundary()
{
    static thread_local size_t part = 1;
    // ----=_Part_{seq}_{tid}.{timestamp-str}
    std::stringstream ss;
    util::get_cur_thread_id_str();
    ss << "----=_Part_" << part++
       << "_" << util::get_cur_thread_id_str()
       << "." << util::get_cur_time_us();
    return ss.str();
}

// Syntax:
//
// multipart-body := [preamble CRLF]
//                   dash-boundary transport-padding CRLF
//                   body-part *encapsulation
//                   close-delimiter transport-padding
//                   [CRLF epilogue]
//
// body-part     := MIME-part-headers [CRLF *OCTET]
// encapsulation := delimiter transport-padding
//                  CRLF body-part
//
// dash-boundary   := "--" boundary
// delimiter       := CRLF dash-boundary
// close-delimiter := delimiter "--"
//
// OCTET := <any 0-255 octet value>
// transport-padding := *LWSP-char
//
// preamble := discard-text
// epilogue := discard-text
// discard-text := *(*text CRLF)
// text = <any CHAR, including bare
//         CR & bare LF, but NOT
//         including CRLF>

// Boundary delimiters must not appear within the encapsulated material,
// and must be no longer than 70 characters, not counting the two
// leading hyphens.

// The boundary delimiter MUST occur at the beginning of a line,
// following a CRLF, and the initial CRLF is considered to be
// attached to the boundary delimiter line rather than part of
// the preceding part.
//
// The boundary may be followed by zero or more characters of
// linear whitespace. It is then terminated by either another CRLF
// and the header fields for the next part, or by two CRLFs,
// in which case there are no header fields for the next part.
//
// NOTE:
// The CRLF preceding the boundary delimiter line is conceptually
// attached to the boundary so that it is possible to have a part that
// does not end with a CRLF (line break).
//
// Body parts that must be considered to end with line breaks, therefore,
// must have two CRLFs preceding the boundary delimiter line, the first
// of which is part of the preceding body part, and the second
// of which is part of the encapsulation boundary.

multipart::multipart(std::string_view subtype)
{
    boundary = generate_boundary();
    std::string content_type("multipart/");
    content_type.append(subtype).append("; boundary=\"").append(boundary).append("\"");
    if (subtype == "form-data") {
        top_level = false;
        return;
    }
    add_header("Content-Type", content_type);
}

multipart& multipart::attach(base *part)
{
    part->top_level = false;
    parts.emplace_back(part);
    return *this;
}

std::string& multipart::str()
{
    data.clear();
    data.append(base::str());
    if (!parts.empty()) {
        // dash-boundary CRLF
        data.append(TWO_HYPHEN).append(boundary).append(CRLF);
    }
    for (auto& part : parts) {
        data.append(part->str());
        // delimiter CRLF
        data.append(CRLF).append(TWO_HYPHEN).append(boundary).append(CRLF);
    }
    // close-delimiter
    data.pop_back();
    data.pop_back();
    data.append(TWO_HYPHEN);
    return data;
}

form_data_writer multipart::new_form_data_writer()
{
    return form_data_writer(this);
}

form_data_writer::form_data_writer(class multipart *mp)
    : multipart(mp)
{
}

void form_data_writer::add_text_part(std::string_view name, std::string_view value)
{
    auto *part = new base();
    part->add_header("Content-Disposition", util::concat("form-data; name=\"", name, "\""));
    part->add_header("Content-Type", "text/plain; charset=utf-8");
    part->encoded_data = value;
    multipart->attach(part);
}

void form_data_writer::add_file_part(std::string_view name, const std::string& pathname)
{
    buffer buf;
    int fd = open(pathname.c_str(), O_RDONLY);
    assert(fd != -1);
    while (buf.read_fd(fd) > 0) ;
    close(fd);

    auto *part = new base();
    const char *media_type = mimetypes.get_mime_type(pathname);
    part->add_header("Content-Disposition",
                     util::concat("form-data; name=\"", name, "\"; filename=\"", pathname, "\""));
    part->add_header("Content-Type", media_type ? media_type : "application/octet-stream");
    part->encoded_data.assign(buf.peek(), buf.readable());
    multipart->attach(part);
}

std::string form_data_writer::get_content_type()
{
    return util::concat("Content-Type: multipart/form-data; boundary=\"", multipart->get_boundary(), "\"");
}

ParseCode codec::parse_header(Headers& headers, buffer& buf)
{
    while (buf.readable() > 0) {
        auto crlf = buf.find_crlf();
        if (crlf < 0) break;

        if (buf.starts_with(CRLF)) {
            buf.retrieve(2);
            return Ok;
        }

        auto pos = buf.find(":");
        if (pos < 0) return Error;

        std::string_view field(buf.peek(), pos);
        std::string_view value(util::trim({buf.peek() + pos + 1, (size_t)(crlf - pos - 1)}));
        // Ignore header with null value.
        if (!value.empty()) {
            headers.emplace(field, value);
        }

        buf.retrieve(crlf + 2);
    }
    return NotEnough;
}

// content := "Content-Type" ":" type "/" subtype
//            *(";" parameter)
//            ; Matching of media type and subtype
//            ; is ALWAYS case-insensitive.
//
// type := discrete-type / composite-type
//
// discrete-type := "text" / "image" / "audio" / "video" /
//                  "application" / extension-token
//
// composite-type := "message" / "multipart" / extension-token
//
// parameter := attribute "=" value
//
// attribute := token
//              ; Matching of attributes
//              ; is ALWAYS case-insensitive.
//
// value := token / quoted-string
//
bool content_type_value::parse(std::string_view value)
{
    auto pos = value.find("/");
    if (pos == value.npos) return false;
    type.assign(value.data(), pos);
    value.remove_prefix(pos + 1);

    pos = value.find(";");
    if (pos == value.npos) {
        subtype.assign(value);
        return true;
    }
    subtype.assign(value.data(), pos);
    value.remove_prefix(pos + 1);

    return codec::parse_parameter(parameters, value);
}

bool codec::parse_parameter(Parameters& parameters, std::string_view str)
{
    while (true) {
        auto p = std::find_if_not(str.begin(), str.end(), isspace);
        if (p == str.end()) return true;
        auto sep = std::find(p, str.end(), '=');
        if (sep == str.end()) return false;
        std::string_view attr(p, sep - p);
        if (++sep == str.end()) return false;
        const char *end;
        if (*sep == '\"') {
            end = parse_quoted_string(++sep, str.end());
            if (end == nullptr) return false;
            std::string_view value(sep, end - sep);
            parameters.emplace(attr, value);
            end = std::find(end, str.end(), ';');
        } else {
            end = std::find(sep, str.end(), ';');
            std::string_view value(sep, end - sep);
            parameters.emplace(attr, util::trim(value));
        }
        if (end == str.end()) break;
        str.remove_prefix(end + 1 - str.begin());
    }
    return true;
}

const char *codec::parse_quoted_string(const char *first, const char *last)
{
    for ( ; first != last; ++first) {
        if (*first == '\\') {
            if (++first == last) return nullptr;
        } else if (*first == '\"') {
            return first;
        }
    }
    return nullptr;
}

ParseCode parser::parse(buffer& buf)
{
    if (!root) {
        auto code = codec::parse_header(headers, buf);
        if (code != Ok) return code;

        auto *node = part_node::get_part_node(headers);
        if (!node) return Error;

        root.reset(node);

        if (root->is_multipart()) {
            state = Preamble;
        }
    }

    if (!root->is_multipart()) {
        return root->parse(buf);
    }

    switch (state) {
    case Preamble:
    {
        // Optional [preamble CRLF]
        auto crlf = buf.find_crlf();
        if (crlf < 0) return NotEnough;
        auto& dash_boundary = dynamic_cast<multipart_node*>(root.get())->dash_boundary;
        if (!buf.starts_with(dash_boundary)) {
            preamble.assign(buf.peek(), crlf);
            buf.retrieve(crlf + 2);
        }
        state = MultipartBody;
    }
    case MultipartBody:
    {
        auto code = root->parse(buf);
        if (code != Ok) return code;
        if (buf.starts_with(CRLF)) {
            buf.retrieve(2);
            state = Epilogue;
        } else {
            return Ok;
        }
    }
    case Epilogue:
    {
        // Optional [CRLF epilogue]
        auto crlf = buf.find_crlf();
        if (crlf < 0) return NotEnough;
        epilogue.assign(buf.peek(), crlf);
        buf.retrieve(crlf + 2);
        return Ok;
    }
    }
}

void parser::iter_parts(iter_part_handler handler)
{
    root->iter_parts(handler);
}

ParseCode part_node::parse(buffer& buf)
{
    return Ok;
}

part_node *part_node::get_part_node(Headers& headers)
{
    auto it = headers.find("Content-Type");
    if (it == headers.end()) {
        // If no Content-Type field is present it is assumed to be
        // "message/rfc822" in a "multipart/digest" and
        // "text/plain; charset=us-ascii" otherwise.
        auto *node = new part_node();
        node->headers.swap(headers);
        return node;
    }

    content_type_value content_type;
    if (!content_type.parse(it->second)) {
        return nullptr;
    }

    it = content_type.parameters.find("boundary");
    if (it == content_type.parameters.end()) {
        auto *node = new part_node();
        node->headers.swap(headers);
        return node;
    }

    auto *node = new multipart_node(it->second);
    node->headers.swap(headers);
    return node;
}

// "Content-Transfer-Encoding: 7bit" is assumed if the
// Content-Transfer-Encoding header field is not present.
bool part_node::decode(std::string_view data)
{
    if (data.empty()) return true;
    auto it = headers.find("Content-Transfer-Encoding");
    if (it == headers.end()) {
        // nothing is to be done.
        return true;
    }
    if (util::equal_case(it->second, "base64")) {
        body = codec::decode_base64(data);
    } else if (util::equal_case(it->second, "quoted-printable")) {
        body = codec::decode_QP(data);
    } else {
        body = data;
    }
    printf("%s\n", body.c_str());
    return !body.empty();
}


// The "mixed" subtype of "multipart" is intended for use when
// the body parts are independent and need to be bundled in a
// particular order.
//
// Any "multipart" subtypes that an implementation does not recognize
// must be treated as being of subtype "mixed".
//
// The "multipart/alternative" type is syntactically identical to
// "multipart/mixed", but the semantics are different. In particular,
// each of the body parts is an "alternative" version of the same
// information.

// LWSP-char = SP / HT ; LWSP(linear-white-space)
static bool islwsp(int c)
{
    return c == SP || c == HT;
}

static void skip_lwsp_char(buffer& buf)
{
    while (buf.readable() > 0) {
        if (!islwsp(*buf.peek())) break;
        buf.retrieve(1);
    }
}

multipart_node::multipart_node(std::string_view boundary)
{
    dash_boundary.assign(TWO_HYPHEN).append(boundary);
    delimiter.assign(CRLF).append(dash_boundary);
}

ParseCode multipart_node::parse(buffer& buf)
{
    switch (state) {
    case DashBoundary:
    {
        // dash-boundary transport-padding CRLF
        auto crlf = buf.find_crlf();
        if (crlf < 0) return NotEnough;
        if (!buf.starts_with(dash_boundary)) return Error;
        if (!std::all_of(buf.peek() + dash_boundary.size(), buf.peek() + crlf, islwsp)) return Error;
        buf.retrieve(crlf + 2);
        state = HeaderPart;
    }
    case HeaderPart:
    {
        if (!cur) {
            auto code = codec::parse_header(part_headers, buf);
            if (code != Ok) return code;

            cur = get_part_node(part_headers);
            if (!cur) return Error;
        }

        if (cur->is_multipart()) {
            auto code = cur->parse(buf);
            if (code != Ok) return code;
        }
        state = BodyPart;
    }
    case BodyPart:
    {
        auto pos = buf.find(delimiter);
        if (pos < 0) return NotEnough;
        if (!cur->decode({buf.peek(), (size_t)pos})) return Error;
        buf.retrieve(pos + delimiter.size());
        state = Delimiter;
    }
    case Delimiter:
    {
        if (buf.readable() < 2) return NotEnough;
        // find out whether it is delimiter or close-delimiter
        if (buf.starts_with(TWO_HYPHEN)) {
            // close-delimiter transport-padding
            buf.retrieve(2);
            skip_lwsp_char(buf);
            parts.emplace_back(cur);
            cur = nullptr;
            return Ok;
        }
        // body-part     := MIME-part-headers [CRLF *OCTET]
        // encapsulation := delimiter transport-padding
        //                  CRLF body-part
        auto crlf = buf.find_crlf();
        if (crlf < 0) return NotEnough;
        if (!std::all_of(buf.peek(), buf.peek() + crlf, islwsp)) return Error;
        buf.retrieve(crlf + 2);

        parts.emplace_back(cur);
        cur = nullptr;

        state = HeaderPart;
        return parse(buf);
    }
    }
}

void part_node::iter_parts(const iter_part_handler& handler)
{
    handler(*this);
}

void multipart_node::iter_parts(const iter_part_handler& handler)
{
    for (auto& part : parts) {
        part->iter_parts(handler);
    }
}

}
}
