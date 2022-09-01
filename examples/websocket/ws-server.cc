#include <angel/server.h>

#include <iostream>

std::string base64_encode(const char *data, size_t len);
std::string sha1(const std::string& data, bool normal);

struct WebSocketContext {
    enum { Handshake, HandshakeOK, HandshakeError, Establish };
    WebSocketContext() : state(Handshake) {  }
    int handshake(const char *line, const char *end);
    void sec_websocket_accept(std::string key);
    void handshake_ok();
    void handshake_error();
    std::string SecWebSocketAccept;
    std::string buffer;
    int state;
};

// Client:
// GET / HTTP/1.1
// Host: www.example.com
// Upgrade: websocket
// Connection: Upgrade
// Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
// Origin: http://example.com
// Sec-WebSocket-Protocol: chat # Optional
// Sec-WebSocket-Version: 13
// ...
//
// Server:
// HTTP/1.1 101 Switching Protocols
// Connection: Upgrade
// Upgrade: websocket
// Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
int WebSocketContext::handshake(const char *line, const char *end)
{
    // std::cout << "handshake: " << std::string(line, end).c_str() << "\n";
    if (strncmp(line, "\r\n", 2) == 0)
        return HandshakeOK;
    if (strncasecmp(line, "GET ", 4) == 0) {
        std::string url;
        const char *p = std::find_if(line + 4, end, isspace);
        url.assign(line + 4, p);
        if (strncasecmp(p + 1, "HTTP/1.1", 8))
            return HandshakeError;
    } else if (strncasecmp(line, "Host: ", 6) == 0) {

    } else if (strncasecmp(line, "Connection: ", 12) == 0) {
        if (strncasecmp(line + 12, "Upgrade", 7))
            return HandshakeError;
    } else if (strncasecmp(line, "Upgrade: ", 9) == 0) {
        if (strncasecmp(line + 9, "websocket", 9))
            return HandshakeError;
    } else if (strncasecmp(line, "Origin: ", 8) == 0) {

    } else if (strncasecmp(line, "Sec-WebSocket-Key: ", 19) == 0) {
        sec_websocket_accept(std::string(line + 19, std::find_if(line + 19, end, isspace)));
    } else if (strncasecmp(line, "Sec-WebSocket-Version: ", 23) == 0) {
        if (atoi(line + 23) != 13)
            return HandshakeError;
    }
    return Handshake;
}

void WebSocketContext::sec_websocket_accept(std::string key)
{
    static const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    key = sha1(key + guid, true);
    SecWebSocketAccept = base64_encode(key.data(), key.size());
}

void WebSocketContext::handshake_ok()
{
    buffer.clear();
    buffer += "HTTP/1.1 101 Switching Protocols\r\n";
    buffer += "Connection: Upgrade\r\n";
    buffer += "Upgrade: websocket\r\n";
    buffer += "Sec-WebSocket-Accept: " + SecWebSocketAccept + "\r\n\r\n";
    std::cout << buffer.c_str();
}

void WebSocketContext::handshake_error()
{
    buffer.clear();
    buffer += "HTTP/1.1 403 Forbidden\r\n\r\n";
    std::cout << buffer.c_str();
}

static void message_handler(const angel::connection_ptr& conn, angel::buffer& buf)
{
    std::cout << buf.c_str() << "\n";
    auto& context = std::any_cast<WebSocketContext&>(conn->get_context());
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        switch (context.state) {
            case WebSocketContext::Handshake:
                context.state = context.handshake(buf.peek(), buf.peek() + crlf);
                switch (context.state) {
                case WebSocketContext::HandshakeOK:
                    buf.retrieve(2);
                    context.state = WebSocketContext::Establish;
                    context.handshake_ok();
                    conn->send(context.buffer);
                    break;
                case WebSocketContext::HandshakeError:
                    context.handshake_error();
                    conn->send(context.buffer);
                    conn->close();
                    return;
                }
                break;
            case WebSocketContext::Establish:
                break;
        }
        buf.retrieve(crlf + 2);
    }
}

class WebSocketServer {
public:
    WebSocketServer(angel::evloop *loop, angel::inet_addr listen_addr)
        : server(loop, listen_addr)
    {
        server.set_connection_handler([](const angel::connection_ptr& conn){
                conn->set_context(WebSocketContext());
                });
        server.set_message_handler(message_handler);
    }
    void start()
    {
        server.start();
    }
private:
    angel::server server;
};

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-------+-+-------------+-------------------------------+
// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
// | |1|2|3|       |K|             |                               |
// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
// |     Extended payload length continued, if payload len == 127  |
// + - - - - - - - - - - - - - - - +-------------------------------+
// |                               |Masking-key, if MASK set to 1  |
// +-------------------------------+-------------------------------+
// | Masking-key (continued)       |          Payload Data         |
// +-------------------------------- - - - - - - - - - - - - - - - +
// :                     Payload Data continued ...                :
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// |                     Payload Data continued ...                |
// +---------------------------------------------------------------+

int main()
{
    angel::evloop loop;
    WebSocketServer ws(&loop, angel::inet_addr(8000));
    ws.start();
    loop.run();
}
