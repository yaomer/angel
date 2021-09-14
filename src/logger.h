#ifndef _ANGEL_LOGGER_H
#define _ANGEL_LOGGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>

#include "buffer.h"
#include "noncopyable.h"

namespace angel {

class logger : noncopyable {
public:
    enum class flush_flags {
        file    = 1,
        stdout  = 2,
    };
    enum class level {
        debug   = 1,
        info    = 2,
        warn    = 3,
        error   = 4,
        fatal   = 5,
    };

    logger();
    ~logger();
    void set_dir(std::string dir);
    void set_name(std::string name);
    void set_level(level level)
    {
        log_level = level;
    }
    void set_flush(flush_flags where)
    {
        flush_to = where;
        restart();
    }
    bool is_filter(level level)
    {
        return level < log_level;
    }
    void format(level level, const char *file, int line, const char *fmt, ...);
    void restart();
    void quit();
private:
    void thread_func();
    std::string get_new_file();
    void create_new_file();
    void roll_file();
    void set_flush();
    void flush();
    void write(const char *s, size_t len);
    const char *format_time();
    const char *get_level_str(level level);

    std::string name;
    buffer write_buf;
    buffer flush_buf;
    std::thread cur_thread;
    std::mutex mlock;
    std::condition_variable condvar;
    std::atomic_bool is_quit;
    int cur_fd;
    std::string dir;
    std::string filename;
    size_t cur_file_size;
    flush_flags flush_to;
    level log_level;
    size_t log_buffer_max_size;
    size_t log_roll_file_size;
    int log_flush_interval;
};

extern angel::logger __logger;

void set_log_dir(std::string dir);
void set_log_name(std::string name);
void set_log_level(logger::level level);
void set_log_flush(logger::flush_flags where);

}

#define log_debug(...) \
    if (!angel::__logger.is_filter(angel::logger::level::debug)) \
        angel::__logger.format(angel::logger::level::debug, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) \
    if (!angel::__logger.is_filter(angel::logger::level::info)) \
        angel::__logger.format(angel::logger::level::info, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) \
    if (!angel::__logger.is_filter(angel::logger::level::warn)) \
        angel::__logger.format(angel::logger::level::warn, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) \
    if (!angel::__logger.is_filter(angel::logger::level::error)) \
        angel::__logger.format(angel::logger::level::error, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) \
    if (!angel::__logger.is_filter(angel::logger::level::fatal)) \
        angel::__logger.format(angel::logger::level::fatal, __FILE__, __LINE__, __VA_ARGS__)

#endif // _ANGEL_LOGGER_H
