#ifndef _ANGEL_HTTP_RESPONSE_H
#define _ANGEL_HTTP_RESPONSE_H

#include <string>
#include <map>

class HttpResponse {
public:
    enum {
        OK = 200,
        NOT_FOUND = 404,
        UNIMPLEMENTED = 501,
    };
    HttpResponse() : _statusCode(0) {  }
    const std::string& buffer() { return _buffer; }
    void setStatusCode(int statusCode)
    { _statusCode = statusCode; }
    void setStatusMessage(const std::string& statusMessage)
    { _statusMessage.assign(statusMessage); }
    void setServer(const std::string& server)
    { _headers["Server"] = server; }
    void setContentType(const std::string& contentType)
    { _headers["Content-Type"] = contentType; }
    void setContentLength(const std::string& contentLength)
    { _headers["Content-Length"] = contentLength; }
    void setBody(const std::string& body)
    { _body.assign(body); }
    void fillResponse();
private:
    int _statusCode;
    std::string _statusMessage;
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::string _buffer;
};

#endif // _ANGEL_HTTP_RESPONSE_H
