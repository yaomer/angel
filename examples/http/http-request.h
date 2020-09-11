#ifndef _ANGEL_HTTP_REQUEST_H
#define _ANGEL_HTTP_REQUEST_H

#include <string>
#include <vector>

class http_request {
public:
    std::string& method() { return _method; }
    std::string& url() { return _url; }
    std::string& version() { return _version; }
    std::string& host() { return _host; }
    std::string& connection() { return _connection; }
    std::vector<std::string>& args() { return _arglist; }
    void set_method(const char *s, const char *es)
    { _method.assign(s, es - s); }
    void set_url(const char *s, const char *es)
    { _url.assign(s, es - s); }
    void set_version(const char *s, const char *es)
    { _version.assign(s, es - s); }
    void set_host(const char *s, const char *es)
    { _host.assign(s, es - s); }
    void set_connection(const char *s, const char *es)
    { _connection.assign(s, es - s); }
    void add_arg(const char *s, const char *es)
    { _arglist.emplace_back(s, es);}
private:
    std::string _method;
    std::string _url;
    std::string _version;
    std::string _host;
    std::string _connection;
    std::vector<std::string> _arglist;
};

#endif // _ANGEL_HTTP_REQUEST_H
