#include <angel/logger.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>

#include <iostream>
#include <chrono>

#include <angel/util.h>

namespace angel {

using namespace util;

angel::logger __logger;

static void log_term_handler(int signo)
{
    log_info("logger ready to exit...");
    __logger.quit();
    fprintf(stderr, "\n");
    exit(1);
}

logger::logger()
    : cur_thread(std::thread([this]{ this->thread_func(); })),
    is_quit(false),
    cur_fd(-1),
    dir(".log/"),
    cur_file_size(0),
    flush_to(flush_flags::file),
    log_level(level::info),
    log_buffer_max_size(512 * 1024),
    log_roll_file_size(1024 * 1024 * 1024),
    log_flush_interval(1)
{
    mkdir(".log", 0777);
    signal(SIGINT, log_term_handler);
    signal(SIGTERM, log_term_handler);
}

logger::~logger()
{
    quit();
}

void logger::set_dir(std::string dir)
{
    if (dir != this->dir) {
        if (mkdir(dir.c_str(), 0777) < 0 && errno != EEXIST)
            log_fatal("mkdir(%s) error: %s", dir.c_str(), strerrno());
        if (dir.back() != '/') dir.push_back('/');
        this->dir = dir;
    }
}

void logger::set_name(std::string name)
{
    this->name = name;
    if (filename != "") {
        unlink(filename.c_str());
        create_new_file();
    }
}

void logger::set_level(level level)
{
    log_level = level;
}

void logger::set_flush(flush_flags where)
{
    flush_to = where;
    restart();
}

bool logger::is_filter(level level)
{
    return level < log_level;
}

std::string logger::get_new_file()
{
    struct tm tm;
    time_t seconds = get_cur_time_ms() / 1000;

    localtime_r(&seconds, &tm);

    char buf[32] = { 0 };
    snprintf(buf, sizeof(buf), "%4d-%02d-%02d-%02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    std::string newfile(dir);
    if (name != "") newfile += name + "-";
    newfile += buf;
    newfile += ".log";

    return newfile;
}

void logger::create_new_file()
{
    filename = get_new_file();
    cur_fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0664);
    if (cur_fd < 0) {
        log_fatal("open(%s) error: %s", filename.c_str(), strerrno());
    }
    cur_file_size = 0;
}

void logger::roll_file()
{
    if (cur_file_size >= log_roll_file_size) {
        close(cur_fd);
        create_new_file();
    }
}

void logger::set_flush()
{
    switch (flush_to) {
    case flush_flags::file:
        create_new_file();
        break;
    case flush_flags::stdout:
        // we don't close stdin(0), stdout(1), stderr(2)
        if (cur_fd > 2) close(cur_fd);
        cur_fd = STDOUT_FILENO;
        break;
    }
}

void logger::thread_func()
{
    set_flush();
    while (true) {
        {
            std::unique_lock<std::mutex> ulock(mlock);
            // 这里就算出现虚假唤醒也无关紧要，即睡眠时间小于log_flush_interval也是可以的
            condvar.wait_for(ulock, std::chrono::seconds(log_flush_interval));
            if (write_buf.readable() > 0)
                write_buf.swap(flush_buf);
        }
        if (flush_buf.readable() > 0) flush();
        if (is_quit) break;
    }
}

void logger::flush()
{
    while (flush_buf.readable() > 0) {
        ssize_t n = ::write(cur_fd, flush_buf.peek(), flush_buf.readable());
        if (n < 0) {
            fprintf(stderr, "write: %s", strerrno());
            break;
        }
        cur_file_size += n;
        flush_buf.retrieve(n);
    }
    if (flush_to == flush_flags::file)
        roll_file();
}

void logger::write(const char *s, size_t len)
{
    std::lock_guard<std::mutex> lock(mlock);
    write_buf.append(s, len);
    if (write_buf.readable() > log_buffer_max_size)
        condvar.notify_one();
}

void logger::quit()
{
    is_quit = true;
    condvar.notify_one();
    if (cur_thread.joinable())
        cur_thread.join();
}

void logger::restart()
{
    quit();
    is_quit = false;
    cur_file_size = 0;
    if (!filename.empty()) {
        remove(filename.c_str());
        filename.clear();
    }
    std::thread new_thread([this]{ this->thread_func(); });
    cur_thread = std::move(new_thread);
}

static thread_local std::string log_cur_tid = get_cur_thread_id_str();
static thread_local const size_t log_cur_tid_len = strlen(log_cur_tid.c_str());

static thread_local char log_time_buf[32];
static thread_local time_t log_last_second = 0;

static thread_local char log_output_buf[65536];

const char *logger::format_time()
{
    struct tm tm;
    long long ms = get_cur_time_ms();
    time_t seconds = ms / 1000;

    if (seconds != log_last_second) {
        localtime_r(&seconds, &tm);
        snprintf(log_time_buf, sizeof(log_time_buf),
                "%4d-%02d-%02d %02d:%02d:%02d.%03lld",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, ms % 1000);
        log_last_second = seconds;
    } else {
        // 在1s内只需要格式化ms部分
        snprintf(log_time_buf + 20, sizeof(log_time_buf) - 20,
                "%03lld", ms % 1000);
    }
    return log_time_buf;
}

const char *logger::get_level_str(level level)
{
    switch (level) {
    case level::debug:
        return "DEBUG: ";
    case level::info:
        return "INFO:  ";
    case level::warn:
        return "WARN:  ";
    case level::error:
        return "ERROR: ";
    case level::fatal:
        return "FATAL: ";
    }
}

void logger::format(level level, const char *file, int line, const char *fmt, ...)
{
    char *ptr = log_output_buf;
    char *eptr = log_output_buf + sizeof(log_output_buf);
    size_t len = 0;
    va_list ap;
    va_start(ap, fmt);
    memcpy(ptr, get_level_str(level), 7);
    ptr += 7;
    memcpy(ptr, format_time(), 23);
    ptr += 23;
    *ptr++ = ' ';
    vsnprintf(ptr, eptr - 256 - ptr, fmt, ap);
    va_end(ap);
    ptr += strlen(ptr);
    memcpy(ptr, " - ", 3);
    ptr += 3;
    len = strlen(file);
    memcpy(ptr, file, len);
    ptr += len;
    *ptr++ = ':';
    snprintf(ptr, eptr - ptr, "%d", line);
    ptr += strlen(ptr);
    memcpy(ptr, " - ", 3);
    ptr += 3;
    memcpy(ptr, log_cur_tid.data(), log_cur_tid_len);
    ptr += log_cur_tid_len;
    *ptr++ = '\n';
    *ptr = '\0';
    write(log_output_buf, ptr - log_output_buf);
    if (level == level::fatal) {
        // raise() will only return after the signal handler has returned
        raise(SIGTERM);
    }
}

void set_log_dir(std::string dir)
{
    __logger.set_dir(dir);
}

void set_log_name(std::string name)
{
    __logger.set_name(name);
}

void set_log_level(logger::level level)
{
    __logger.set_level(level);
}

void set_log_flush(logger::flush_flags where)
{
    __logger.set_flush(where);
}

}
