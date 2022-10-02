#include <angel/mime.h>

namespace angel {
namespace mime {

static const std::unordered_map<std::string_view, int> charset_map = {
    { "iso-8859-1",  charset::QP },
    { "iso-8859-2",  charset::QP },
    { "iso-8859-3",  charset::QP },
    { "iso-8859-4",  charset::QP },
    { "iso-8859-9",  charset::QP },
    { "iso-8859-10", charset::QP },
    { "iso-8859-13", charset::QP },
    { "iso-8859-14", charset::QP },
    { "iso-8859-15", charset::QP },
    { "iso-8859-16", charset::QP },
    { "gb2312",      charset::BASE64 },
    { "gbk",         charset::BASE64 },
    { "gb18030",     charset::BASE64 },
    { "big5",        charset::BASE64 },
    { "euc-jp",      charset::BASE64 },
    { "shift_jis",   charset::BASE64 },
    { "utf-8",       charset::BASE64 },
    { "us-ascii",    charset::BIT7 },
};

static const std::unordered_map<std::string_view, std::string_view> alias_map = {
    { "latin_1", "iso-8859-1" },
    { "latin-1", "iso-8859-1" },
    { "latin_2", "iso-8859-2" },
    { "latin-2", "iso-8859-2" },
    { "latin_3", "iso-8859-3" },
    { "latin-3", "iso-8859-3" },
    { "latin_4", "iso-8859-4" },
    { "latin-4", "iso-8859-4" },
    { "latin_5", "iso-8859-9" },
    { "latin-5", "iso-8859-9" },
    { "latin_6", "iso-8859-10" },
    { "latin-6", "iso-8859-10" },
    { "latin_7", "iso-8859-13" },
    { "latin-7", "iso-8859-13" },
    { "latin_8", "iso-8859-14" },
    { "latin-8", "iso-8859-14" },
    { "latin_9", "iso-8859-15" },
    { "latin-9", "iso-8859-15" },
    { "latin_10","iso-8859-16" },
    { "latin-10","iso-8859-16" },
    { "euc_jp",  "euc-jp" },
    { "ascii",   "us-ascii" },
};

int charset::get_encoding(std::string_view charset)
{
    auto it = charset_map.find(charset);
    if (it != charset_map.end()) return it->second;
    const char *canonical = get_canonical(charset);
    if (canonical) return get_encoding(canonical);
    return NONE;
}

const char *charset::get_canonical(std::string_view charset)
{
    auto it = alias_map.find(charset);
    if (it != alias_map.end()) return it->second.data();
    else return nullptr;
}

}
}
