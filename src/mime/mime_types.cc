#include <angel/mime.h>

namespace angel {
namespace mime {

static const char *get_mime_types_file()
{
    static const std::vector<std::string> known_files = {
        "/etc/mime.types",
        "/etc/httpd/mime.types",
        "/etc/httpd/conf/mime.types",
        "/etc/apache/mime.types",
        "/etc/apache2/mime.types",
        "/usr/local/etc/httpd/conf/mime.types",
        "/usr/local/lib/netscape/mime.types",
        "/usr/local/etc/mime.types",
    };

    for (auto& file : known_files) {
        if (util::is_regular_file(file)) {
            return file.c_str();
        }
    }
    // Can not reach here.
    fprintf(stderr, "No known mime.types file\n");
    abort();
}

mimetypes::mimetypes()
{
    auto res = util::parse_conf(get_mime_types_file());
    for (auto& args : res) {
        for (int i = 1; i < args.size(); i++) {
            mime_types[args[0]].emplace_back(args[i]);
            file_extensions.emplace(args[i], args[0]);
        }
    }
}

void mimetypes::load(const std::string& path)
{
    auto res = util::parse_conf(path.c_str());
    for (auto& args : res) {
        auto it = mime_types.find(args[0]);
        if (it != mime_types.end()) {
            for (auto& ext : it->second) {
                file_extensions.erase(ext);
            }
            mime_types.erase(it);
        }
        for (int i = 1; i < args.size(); i++) {
            mime_types[args[0]].emplace_back(args[i]);
            file_extensions.emplace(args[i], args[0]);
        }
    }
}

static std::string get_suffix(std::string_view filename)
{
    std::string suffix;
    auto pos = filename.rfind(".");
    if (pos != filename.npos) suffix.assign(filename.substr(pos + 1));
    return suffix;
}

const char *mimetypes::get_mime_type(const std::string& filename) const
{
    auto suffix = get_suffix(filename);
    if (suffix == "") return nullptr;
    auto it = file_extensions.find(suffix);
    return it != file_extensions.end() ? it->second.c_str() : nullptr;
}

const std::vector<std::string> *
mimetypes::get_file_extensions(const std::string& mime_type) const
{
    auto it = mime_types.find(mime_type);
    return it != mime_types.end() ? &it->second : nullptr;
}

}
}
