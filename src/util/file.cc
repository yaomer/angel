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

bool copy_file(const char *from, const char *to)
{
    char buf[4096];
    bool rc = true;

    if (strcmp(from, to) == 0) return rc;

    int fd1 = open(from, O_RDONLY);
    int fd2 = open(to, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd1 < 0 || fd2 < 0) goto end;

    while (true) {
        ssize_t n = read(fd1, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
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

bool is_regular_file(const char *path)
{
    struct stat st;
    ::stat(path, &st);
    return S_ISREG(st.st_mode);
}

bool is_directory(const char *path)
{
    struct stat st;
    ::stat(path, &st);
    return S_ISDIR(st.st_mode);
}

}
}
