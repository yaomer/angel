#ifndef __ANGEL_HTTPLIB_UTIL_H
#define __ANGEL_HTTPLIB_UTIL_H

#include <string>

namespace angel {
namespace httplib {

// Wed, 15 Nov 1995 06:25:24 GMT
std::string format_date();
// Check rfc1123-date
bool check_date_format(std::string_view date);

// Get the last modification time of the file. (microseconds)
int64_t get_last_modified(const std::string& path);
std::string format_last_modified(int64_t msecs);

bool is_etag(std::string_view etag);
bool strong_etag_equal(std::string_view etag1, std::string_view etag2);
bool weak_etag_equal(std::string_view etag1, std::string_view etag2);

std::string generate_file_etag(int64_t last_modified_time, off_t filesize);
std::string generate_file_etag(const std::string& path, off_t filesize);
std::string generate_etag(std::string_view data);

}
}

#endif // __ANGEL_HTTPLIB_UTIL_H
