#include <angel/util.h>

#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <sstream>

#include <angel/logger.h>

namespace angel {
namespace util {

const char *strerr(int err)
{
    static thread_local char errno_buf[256];
    strerror_r(err, errno_buf, sizeof(errno_buf));
    return errno_buf;
}

const char *strerrno()
{
    return strerr(errno);
}

size_t get_cur_thread_id()
{
    return std::hash<std::thread::id>()(std::this_thread::get_id());
}

std::string get_cur_thread_id_str()
{
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

int64_t get_cur_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int64_t get_cur_time_us()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int get_ncpus()
{
    return std::thread::hardware_concurrency();
}

void set_thread_affinity(pthread_t tid, int cpu_number)
{
#if defined (__APPLE__)
    log_warn("OS X Not support to set thread affinity");
#elif defined (__linux__)
    assert(cpu_number < get_ncpus());
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_number, &mask);
    if (pthread_setaffinity_np(tid, sizeof(mask), &mask) < 0)
        log_error("pthread_setaffinity_np error: %s", strerrno());
#endif
}

bool check_ip(std::string_view ipv4_addr)
{
    auto fields = split(ipv4_addr, '.');
    if (fields.size() != 4) return false;
    for (auto& s : fields) {
        if (s.empty()
         || !is_digits(s)
         || (s.size() > 1 && s[0] == '0')
         || svtoi(s).value_or(256) > 255)
            return false;
    }
    return true;
}

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

void daemon()
{
    pid_t pid;

    if ((pid = fork()) < 0) {
        log_fatal("fork: %s", strerrno());
    }
    if (pid > 0)
        exit(0);

    if (setsid() < 0) {
        log_fatal("setsid: %s", strerrno());
    }
    umask(0);

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_fatal("open /dev/null: %s", strerrno());
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
        log_fatal("dup2 stdin: %s", strerrno());
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        log_fatal("dup2 stdout: %s", strerrno());
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        log_fatal("dup2 stderr: %s", strerrno());
    }

    if (fd > STDERR_FILENO)
        close(fd);
}

static thread_local char format_send_buf[65536];

format_result format(const char *fmt, va_list ap)
{
    format_result res;

    // Get the length of the formatted string.
    va_list ap1;
    va_copy(ap1, ap);
    int len = vsnprintf(nullptr, 0, fmt, ap1);
    va_end(ap1);

    if (len + 1 < sizeof(format_send_buf)) {
        res.buf = format_send_buf;
        res.alloced = false;
    } else {
        res.buf = new char[len + 1];
        res.alloced = true;
    }
    vsnprintf(res.buf, len + 1, fmt, ap);
    res.len = len;
    return res;
}

}
}
