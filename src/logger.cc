#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>

#include <iostream>
#include <chrono>

#include "logger.h"
#include "util.h"

using namespace angel;
using namespace angel::util;

namespace angel {

    angel::logger __logger;
}

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

#define TIME_ZONE_OFF 8

std::string logger::get_filename()
{
    struct tm tm;
    time_t seconds = get_cur_time_ms() / 1000;

    gmtime_r(&seconds, &tm);
    tm.tm_hour += TIME_ZONE_OFF;

    char buf[32] = { 0 };
    snprintf(buf, sizeof(buf), "%4d-%02d-%02d-%02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    std::string filename(".log/");
    filename += buf;
    filename += ".log";

    return filename;
}

void logger::create_new_file()
{
    filename = get_filename();
    cur_fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0664);
    if (cur_fd < 0) {
        log_warn("can't open %s: %s, now write to stdout", filename.c_str(), strerrno());
        flush_to = flush_flags::stdout;
        cur_fd = STDOUT_FILENO;
        return;
    }
    cur_file_size = 0;
}

void logger::roll_file()
{
    if (cur_file_size >= log_roll_file_size) {
        ::close(cur_fd);
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
        cur_fd = STDOUT_FILENO;
        break;
    case flush_flags::stderr:
        cur_fd = STDERR_FILENO;
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

namespace angel {

    static thread_local std::string log_cur_tid = get_cur_thread_id_str();
    static thread_local const size_t log_cur_tid_len = strlen(log_cur_tid.c_str());

    static thread_local char log_time_buf[32];
    static thread_local time_t log_last_second = 0;

    static thread_local char log_output_buf[65536];
}

const char *logger::format_time()
{
    struct tm tm;
    long long ms = get_cur_time_ms();
    time_t seconds = ms / 1000;

    if (seconds != log_last_second) {
        gmtime_r(&seconds, &tm);
        tm.tm_hour += TIME_ZONE_OFF;
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

void logger::format(level level, const char *file, int line,
        const char *func, const char *fmt, ...)
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
    len = strlen(func);
    memcpy(ptr, func, len);
    ptr += len;
    memcpy(ptr, ": ", 2);
    ptr += 2;
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

void angel::set_log_level(logger::level level)
{
    __logger.set_level(level);
}

void angel::set_log_flush(logger::flush_flags flag)
{
    __logger.set_flush(flag);
}
