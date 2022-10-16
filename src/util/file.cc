#include <angel/util.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <angel/logger.h>

namespace angel {
namespace util {

ConfigParamlist parse_conf(const char *pathname)
{
    char buf[4096];
    FILE *fp = fopen(pathname, "r");
    if (!fp) {
        log_fatal("can't open %s: %s", pathname, util::strerrno());
    }
    ConfigParamlist paramlist;
    while (fgets(buf, sizeof(buf), fp)) {
        const char *s = buf;
        const char *es = buf + strlen(buf);
        ConfigParam param;
        do {
            s = std::find_if_not(s, es, isspace);
            if (s == es || s[0] == '#') break;
            const char *p = std::find_if(s, es, isspace);
            assert(p != es);
            param.emplace_back(s, p);
            s = p + 1;
        } while (true);
        if (!param.empty())
            paramlist.emplace_back(param);
    }
    fclose(fp);
    return paramlist;
}

ssize_t read_file(int fd, const char *buf, size_t len)
{
    size_t nbytes = len;
    while (len > 0) {
        ssize_t n = read(fd, (void*)buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0) break;
        buf += n;
        len -= n;
    }
    return nbytes - len;
}

bool write_file(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        buf += n;
        len -= n;
    }
    return true;
}

bool copy_file(const std::string& from, const std::string& to)
{
    char buf[4096];
    bool rc = true;

    if (from == to) return rc;

    int fd1 = open(from.c_str(), O_RDONLY);
    int fd2 = open(to.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd1 < 0 || fd2 < 0) goto end;

    while (true) {
        ssize_t n = read_file(fd1, buf, sizeof(buf));
        if (n < 0) {
            rc = false;
            break;
        }
        if (n == 0) break;
        if (!write_file(fd2, buf, n)) {
            rc = false;
            break;
        }
    }
end:
    close(fd1);
    close(fd2);
    return rc;
}

bool is_regular_file(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    return S_ISREG(st.st_mode);
}

bool is_directory(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    return S_ISDIR(st.st_mode);
}

off_t get_file_size(const std::string& path)
{
    struct stat st;
    ::stat(path.c_str(), &st);
    return st.st_size;
}

off_t get_file_size(int fd)
{
    struct stat st;
    ::fstat(fd, &st);
    return st.st_size;
}

off_t page_aligned(off_t offset)
{
    static const int pagesize = getpagesize();

    return (offset / pagesize) * pagesize;
}

}
}
