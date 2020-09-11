#ifndef _ANGEL_HTTP_RESPONSE_H
#define _ANGEL_HTTP_RESPONSE_H

#include <string>
#include <map>

class http_response {
public:
    enum {
        OK = 200,
        NOT_FOUND = 404,
        UNIMPLEMENTED = 501,
    };
    std::string& response() { return buffer; }
    void set_status_code(int code)
    { status_code = code; }
    void set_status_message(const std::string& message)
    { status_message.assign(message); }
    void set_server(const std::string& server)
    { headers["Server"] = server; }
    void set_content_type(const std::string& type)
    { headers["Content-Type"] = type; }
    void set_body(const std::string& data)
    { body.assign(data); }
    void fill_response()
    {
        char buf[64];

        snprintf(buf, sizeof(buf), "HTTP/1.0 %d ", status_code);
        buffer.append(buf)
              .append(status_message)
              .append("\r\n");

        if (!body.empty()) {
            snprintf(buf, sizeof(buf), "%zu", body.size());
            headers["Content-Length"] = buf;
        }

        struct tm tm;
        time_t now = time(nullptr);
        localtime_r(&now, &tm);
        strftime(buf, sizeof(buf), "%a, %b %d, %Y %X-%Z", &tm);
        headers["Date"] = buf;

        for (auto& it : headers) {
            buffer.append(it.first)
                  .append(": ")
                  .append(it.second)
                  .append("\r\n");
        }
        buffer.append("\r\n");

        if (!body.empty())
            buffer.append(body);
    }
    void clear()
    {
        body.clear();
        headers.clear();
        buffer.clear();
    }
private:
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string buffer;
};

#endif // _ANGEL_HTTP_RESPONSE_H
