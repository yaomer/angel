#ifndef _ANGEL_HTTP_REQUEST_H
#define _ANGEL_HTTP_REQUEST_H

#include <string>
#include <vector>

class HttpRequest {
public:
    typedef std::vector<std::string> ArgList;

    std::string& method() { return _method; }
    std::string& url() { return _url; }
    std::string& version() { return _version; }
    std::string& host() { return _host; }
    std::string& connection() { return _connection; }
    ArgList& args() { return _argList; }
    void setMethod(const char *s, const char *es) 
    { _method.assign(s, es - s); }
    void setUrl(const char *s, const char *es)
    { _url.assign(s, es - s); }
    void setVersion(const char *s, const char *es)
    { _version.assign(s, es - s); }
    void setHost(const char *s, const char *es)
    { _host.assign(s, es - s); }
    void setConnection(const char *s, const char *es)
    { _connection.assign(s, es - s); }
    void addArg(const char *s, const char *es)
    { _argList.push_back(std::string(s, es - s));}
private:
    std::string _method;
    std::string _url;
    std::string _version;
    std::string _host;
    std::string _connection;
    ArgList _argList;
};

#endif // _ANGEL_HTTP_REQUEST_H
